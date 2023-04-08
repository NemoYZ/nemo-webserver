#pragma once

#include <soci/soci.h>

#include "orm/mapper.h"

#include <stdint.h>

#include <vector>
#include <unordered_map>
#include <iterator>
#include <type_traits>

#include <google/protobuf/message.h>

namespace nemo {
namespace orm {

namespace detail {

typedef std::unordered_map<int64_t, google::protobuf::Message*> MessageMap;
typedef std::vector<MessageMap> MessageTree;

std::pair<google::protobuf::Message*, bool> 
InnerMakeMessage(const soci::row& r, unsigned int columnNum,
    const tinyxml2::XMLElement* element,
    MessageTree& messageTree, size_t treeIndex);

} // namespace detail

template<typename Iter>
Iter MakeMessage(const soci::rowset<>& rowSet, 
    const tinyxml2::XMLElement* mapperElement, 
    Iter dest) {
    typename detail::MessageTree messageTree;
    for (auto iter = rowSet.begin(); iter != rowSet.end(); ++iter) {
        std::pair<google::protobuf::Message*, bool> myPair = detail::InnerMakeMessage(*iter, 0, 
            mapperElement->FirstChildElement(), messageTree, 0);
        if (myPair.second) {
            *dest = std::unique_ptr<google::protobuf::Message>(myPair.first);
            ++dest;
        }
    }

    return dest;
}

template<typename Iter>
Iter MakeMessage(const soci::rowset<>& rowSet,
    const Mapper* mapper,
    Iter dest) {
    return MakeMessage(rowSet, mapper->getElement(), dest);
}

template<typename Iter>
Iter MakeMessage(const soci::rowset<>& rowSet,
    const String& mapperName,
    Iter dest) {
    return MakeMessage(rowSet, MapperManager::GetInstance().getMapper(mapperName), dest);
}

} // namespace orm
} // namespace nemo