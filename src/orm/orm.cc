#include "orm/orm.h"

namespace nemo {
namespace orm {

namespace detail {

static void SetId(google::protobuf::Message* message, int64_t id) {
    const google::protobuf::Reflection* reflection = message->GetReflection();
    const google::protobuf::Descriptor* descriptor = message->GetDescriptor();
    const google::protobuf::FieldDescriptor* fieldDescriptor = descriptor->field(0);
    reflection->SetInt64(message, fieldDescriptor, id);
}

static void ParseField(google::protobuf::Message* message, 
const soci::row& r, const unsigned int columnNum, int fieldNum) {
    using namespace google::protobuf;
    if (r.get_indicator(columnNum) == soci::indicator::i_null) {
        return;
    }

    const Reflection* reflection = message->GetReflection();
    const google::protobuf::Descriptor* descriptor = message->GetDescriptor();
    const google::protobuf::FieldDescriptor* fieldDescriptor = descriptor->field(fieldNum);

    switch (fieldDescriptor->cpp_type()) {
        case FieldDescriptor::CPPTYPE_BOOL:
            reflection->SetBool(message, fieldDescriptor, r.get<int>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
            reflection->SetDouble(message, fieldDescriptor, r.get<double>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_ENUM: {
            const google::protobuf::EnumDescriptor* enumDescriptor = fieldDescriptor->enum_type();
            const google::protobuf::EnumValueDescriptor* enumValueDescriptor = 
                enumDescriptor->FindValueByNumber(r.get<int>(columnNum));
            reflection->SetEnum(message, fieldDescriptor, enumValueDescriptor);   
        }
            break;
        case FieldDescriptor::CPPTYPE_FLOAT:
            reflection->SetFloat(message, fieldDescriptor, r.get<double>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_INT32:
            reflection->SetInt32(message, fieldDescriptor, r.get<int>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_INT64:
            reflection->SetInt64(message, fieldDescriptor, r.get<long long>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
            //TODO
            break;
        case FieldDescriptor::CPPTYPE_STRING:
            reflection->SetString(message, fieldDescriptor, r.get<std::string>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_UINT32:
            reflection->SetUInt32(message, fieldDescriptor, r.get<int>(columnNum));
            break;
        case FieldDescriptor::CPPTYPE_UINT64:
            reflection->SetUInt64(message, fieldDescriptor, r.get<unsigned long long>(columnNum));
            break;
        default:
            break;
    }
}

static void InitMessage(google::protobuf::Message* message, 
    const tinyxml2::XMLElement* element,
    const soci::row& r, unsigned int columnNum, int fieldStart,
    MessageTree& messageTree, size_t treeIndex) {
    using namespace google::protobuf;

    constexpr std::array<std::string_view, 3> elementNames{"result", "association", "collection"};
    const Reflection* reflection = message->GetReflection();
    const Descriptor* descriptor = message->GetDescriptor();

    for (; columnNum < r.size(); ++columnNum, ++fieldStart) {
        if (::strncasecmp(elementNames[0].data(), element->Name(), elementNames[0].size()) == 0) {
            ParseField(message, r, columnNum, fieldStart);
        } else if (::strncasecmp(elementNames[1].data(), element->Name(), elementNames[1].size()) == 0) {
            std::pair<Message*, bool> myPair = InnerMakeMessage(r, columnNum, element, messageTree, treeIndex + 1);
            if (myPair.second) {
                const FieldDescriptor* fieldDescriptor = descriptor->field(fieldStart);
                reflection->AddAllocatedMessage(message, fieldDescriptor, myPair.first);
            }
            break;
        } else if (::strncasecmp(elementNames[2].data(), element->Name(), elementNames[2].size()) == 0) {
            std::pair<Message*, bool> myPair = InnerMakeMessage(r, columnNum, element, messageTree, treeIndex + 1);
            if (myPair.second) {
                const FieldDescriptor* fieldDescriptor = descriptor->field(fieldStart);
                reflection->AddAllocatedMessage(message, fieldDescriptor, myPair.first);
            }
            break;
        } else {
            assert(false);
        }
        element = element->NextSiblingElement();
    }
}

std::pair<google::protobuf::Message*, bool> 
InnerMakeMessage(const soci::row& r, unsigned int columnNum,
        const tinyxml2::XMLElement* element,
        MessageTree& messageTree, size_t treeIndex) {
    using namespace google::protobuf;
    using namespace tinyxml2;

    bool isNewMessage = false;
    Message* newMessage = nullptr;

    if (columnNum < r.size() && r.get_indicator(columnNum) != soci::indicator::i_null) {
        const char* messageName = nullptr;
        XMLError err = element->QueryAttribute("protobuf_message", &messageName);
        assert(err == XMLError::XML_SUCCESS);
        element = element->FirstChildElement();

        const Descriptor* descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(messageName);
        assert(descriptor); //__builtn_expect
        const Message* protoType = MessageFactory::generated_factory()->GetPrototype(descriptor);
        assert(protoType);
        assert(::strncasecmp(element->Name(), "id", 2) == 0);

        static_assert(sizeof(int64_t) == sizeof(long long), "type size mismatch");
        int64_t id = r.get<long long>(columnNum); //id需要单独处理
        if (treeIndex == messageTree.size()) {
            isNewMessage = true;
            newMessage = protoType->New();
            SetId(newMessage, id);
            messageTree.resize(treeIndex + 1);
            messageTree[treeIndex][id] = newMessage;
        } else {
            auto iter = messageTree[treeIndex].find(id);
            if (iter != messageTree[treeIndex].end()) {
                newMessage = iter->second;
            } else {
                isNewMessage = true;
                newMessage = protoType->New();
                SetId(newMessage, id);
                messageTree[treeIndex][id] = newMessage;
            }
        }
        
        InitMessage(newMessage, element->NextSiblingElement(), 
            r, columnNum + 1, 1, messageTree, treeIndex);
    }

    return std::make_pair(newMessage, isNewMessage);
}

} // namespace detail

} // namespace orm
} // namespace nemo