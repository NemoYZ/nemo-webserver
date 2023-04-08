#include "container/byte_array.h"

#include <malloc.h>
#include <stdio.h>

#include <limits>
#include <iomanip>

#include "log/log.h"
#include "util/util.h"
#include "util/file_appender.h"
#include "system/endian.h"
#include "common/macro.h"

namespace nemo {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

ByteArray::Node* ByteArray::allocateNode() {
    Node* newNode = static_cast<Node*>(::malloc(blockSize_ + sizeof(Node)));
    newNode->next = nullptr;
    ++nodeCount_;
    return newNode;
}

void ByteArray::deallocateNode(Node* node) {
    ::free(node);
}

void ByteArray::ensureWritableSize(size_t size) {
    NEMO_ASSERT(blockSize_ >= lastNodeBytesUsed_);
    size_t lastNodeBytesLeft = static_cast<size_t>(blockSize_ - lastNodeBytesUsed_);
    if (size <= lastNodeBytesLeft) {
        return;
    }   
    // 除去最后节点可写字节数，还需要写的字节数
    size_t sizeNeeded = size - lastNodeBytesLeft;
    // 可以用div指令，不过编译器会优化，不必多此一举
    size_t nodesNeeded = sizeNeeded / blockSize_;
    // 向上取整
    if (sizeNeeded % blockSize_ != 0) {
        // 需要再加一个节点
        ++nodesNeeded;
    }

    Node* curr = lastNode_;
    while (nodesNeeded) {
        Node* tmp = allocateNode();
        Link(curr, tmp);
        curr = tmp;
        --nodesNeeded;
    }
}

ByteArray::ByteArray(const int blockSize) :
    firstNode_(allocateNode()),
    lastNode_(firstNode_),
    currentNode_(firstNode_),
    nodeCount_(1),
    currNodeIndex_(0),
    currNodePosition_(0),
    lastNodeBytesUsed_(0),
    blockSize_(blockSize),
    endian_(std::endian::native) {
}

ByteArray::~ByteArray() {
    Node* node = firstNode_;
    while (node) {
        Node* tmp = node;
        node = static_cast<Node*>(node->next);
        deallocateNode(tmp);
    }
}

void ByteArray::writeFixed(int8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(uint8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(int16_t value) {
    if(endian_ != std::endian::native) {
        value = ByteSwap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(uint16_t value) {
    if(endian_ != std::endian::native) {
        value = ByteSwap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(int32_t value) {
    if(endian_ != std::endian::native) {
        value = ByteSwap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(uint32_t value) {
    if(endian_ != std::endian::native) {
        value = ByteSwap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(int64_t value) {
    if(endian_ != std::endian::native) {
        value = ByteSwap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFixed(uint64_t value) {
    if(endian_ != std::endian::native) {
        value = ByteSwap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::write(int32_t value) {
    write(EncodeZigzag32(value));
}

void ByteArray::write(uint32_t value) {
    /**
	 * 为什么长度是5，而不是4？
	 * 因为我们是每次取7个bit，而不是每次取8bit
	 * 为什么不每次取8bit，而是每次取7bit？
	 * 我们需要用第8bit来标识还有没有数据，
	 * 所以我们在压缩7bit时，需要额外的1bit来表示信息
	 */
    Byte tmp[5]{0};
    uint8_t i = 0;
    while(value >= 0x80) {
        tmp[i++] = static_cast<Byte>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    tmp[i++] = static_cast<Byte>(value);
    write(tmp, i);
}

void ByteArray::write(int64_t value) {
    write(EncodeZigzag64(value));
}

void ByteArray::write(uint64_t value) {
    Byte tmp[10]{0};
    uint8_t i = 0;
    while(value >= 0x80) {
        tmp[i++] = static_cast<Byte>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    tmp[i++] = static_cast<Byte>(value);
    write(tmp, i);
}

void ByteArray::write(float value) {
    static_assert(sizeof(uint32_t) == sizeof(float), 
        "sizeof(uint32_t) != sizeof(float)");
    uint32_t tmp;
    ::memcpy(&tmp, &value, sizeof(tmp));
    writeFixed(tmp);
}

void ByteArray::write(double value) {
    static_assert(sizeof(uint64_t) == sizeof(double), 
        "sizeof(uint64_t) != sizeof(double)");
    uint64_t tmp;
    memcpy(&tmp, &value, sizeof(tmp));
    writeFixed(tmp);
}

void ByteArray::writeFixedString16(StringArg value) {
    writeFixed(static_cast<uint16_t>(value.size()));
    write(value.data(), static_cast<uint16_t>(value.size()));
}

void ByteArray::writeFixedString32(StringArg value) {
    writeFixed(static_cast<uint32_t>(value.size()));
    write(value.data(), static_cast<uint32_t>(value.size()));
}

void ByteArray::writeFixedString64(StringArg value) {
    writeFixed(static_cast<uint32_t>(value.size()));
    write(value.data(), static_cast<uint32_t>(value.size()));
}

void ByteArray::writeVariantStringint(StringArg value) {
    using SizeType = decltype(value.size());
    if (value.size() <= static_cast<SizeType>(
            std::numeric_limits<uint16_t>::max())) {
        writeFixedString16(value);
    } else if (value.size() <= static_cast<SizeType>(
                std::numeric_limits<uint32_t>::max())) {
        writeFixedString32(value);
    } else {
        writeFixedString64(value);
    }
}

void ByteArray::writeString(StringArg value) {
    write(value.data(), value.size());
}

int8_t ByteArray::readFixedInt8() {
    int8_t value;
    read(&value, sizeof(value));
    return value;
}

uint8_t ByteArray::readFixedUint8() {
    uint8_t value;
    read(&value, sizeof(value));
    return value;
}

template<typename T>
static T ReadFixedAux(ByteArray* byteArray) {
    T value;
    byteArray->read(&value, sizeof(value));
    if (std::endian::native == byteArray->getEndian()) {
        return value;
    } else {
        return ByteSwap(value);
    }
}

int16_t ByteArray::readFixedInt16() {
    return ReadFixedAux<int16_t>(this);
}

uint16_t ByteArray::readFixedUint16() {
    return ReadFixedAux<uint16_t>(this);
}

int32_t ByteArray::readFixedInt32() {
    return ReadFixedAux<int32_t>(this);
}

uint32_t ByteArray::readFixedUint32() {
    return ReadFixedAux<uint32_t>(this);
}

int64_t ByteArray::readFixedInt64() {
    return ReadFixedAux<int64_t>(this);
}

uint64_t ByteArray::readFixedUint64() {
    return ReadFixedAux<uint64_t>(this);
}

int32_t ByteArray::readInt32() {
    return DecodeZigzag32(readUint32());
}

template<typename T>
static T ReadAux(ByteArray* byteArray) {
    T value{0};
    for (size_t i = 0; i < sizeof(value); i += 7) {
        uint8_t tmp = byteArray->readFixedUint8();
        if(tmp < 0x80) {	//后面没有数据了
            value |= (static_cast<uint32_t>(tmp) << i);
            break;
        } else {
            value |= ((static_cast<uint32_t>(tmp & 0x7F)) << i); //需要去掉最高位的1bit
        }
    }

    return value;
}

uint32_t ByteArray::readUint32() {
    return ReadAux<uint32_t>(this);
}

int64_t ByteArray::readInt64() {
    return DecodeZigzag64(readUint64());
}

uint64_t ByteArray::readUint64() {
    return ReadAux<uint64_t>(this);
}

float ByteArray::readFloat() {
    static_assert(sizeof(uint32_t) == sizeof(float), 
        "sizeof(uint32_t) != sizeof(float)");
    uint32_t tmp = readFixedUint32();
    float value;
    ::memcpy(&value, &tmp, sizeof(value));
    return value;
}

double ByteArray::readDouble() {
    static_assert(sizeof(uint64_t) == sizeof(double), 
        "sizeof(uint64_t) != sizeof(double)");
    uint64_t tmp = readFixedUint64();
    double value;
    ::memcpy(&value, &tmp, sizeof(value));
    return value;
}

String ByteArray::readFixedString16() {
    uint16_t len = readFixedUint16();
    String buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

String ByteArray::readFixedString32() {
    uint32_t len = readFixedUint32();
    String buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

String ByteArray::readFixedString64() {
    uint64_t len = readFixedUint64();
    String buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

String ByteArray::readVariantStringint() {
    uint64_t len = readUint64();
    String buff;
    buff.resize(len);
    read(&buff[0], len);
    return buff;
}

void ByteArray::clear() {
    Node* node = static_cast<Node*>(firstNode_->next);
    firstNode_->next = nullptr;
    while(node) {
        Node* tmp = node;
        node = static_cast<Node*>(node->next);
        deallocateNode(tmp);
    }
    currentNode_ = firstNode_;
    currNodeIndex_ = 0;
    lastNode_ = firstNode_;
    lastNodeBytesUsed_ = 0;
    currNodePosition_ = 0;
    nodeCount_ = 1;
}

void ByteArray::write(const void* buf, size_t size) {
    if(0 == size) {
        return;
    }
    ensureWritableSize(size);

    //最后一个节点内数据块空余开始位置
    size_t nodePos = lastNodeBytesUsed_;	
    //最后一个节点内数据块剩余大小
    size_t nodeBytesLeft = blockSize_ - lastNodeBytesUsed_;
    //缓冲区写的开始位置		
    size_t bufToWritePos = 0;						    

    while(size > 0) {
        if(nodeBytesLeft >= size) {	//剩余的内存够写下buf
            ::memcpy(lastNode_->data + nodePos, 
                    (const char*)buf + bufToWritePos, size);
            //往后移动size大小
            nodePos += size;					
            break;
        } else { //当前块写不下，需要写到下一块
            //先把当前数据块写满
            ::memcpy(lastNode_->data + nodePos, 
                (const char*)buf + bufToWritePos, nodeBytesLeft);	
            bufToWritePos += nodeBytesLeft;			//缓冲区指针向后移动nodeCap
            size -= nodeBytesLeft;			//剩余大小减去nodeCap
            lastNode_ = static_cast<Node*>(lastNode_->next);	//因为还没有写完，移动到下一个结点继续写
            nodeBytesLeft = blockSize_;		//因为到了新的结点，剩余数据块大小就是一个完整数据块大小
            nodePos = 0;		            //到了新的结点，当前数据块内指针指向首位置
        }
    }

    // 到这里, lastNode_->next 一定为nullptr
    lastNodeBytesUsed_ = nodePos;
}

void ByteArray::read(void* buf, size_t size) {
    if(size > getReadableBytes()) {
        throw std::out_of_range("not enough len");
    }

    size_t nodePos = currNodePosition_;	//节点下标
    size_t nodeBytesLeft = blockSize_ - currNodePosition_;	//节点容量
    size_t bufPos = 0;						    //缓冲区下标

    while(size > 0) {	
        if(nodeBytesLeft > size) {			//在该节点就可以读取剩下要读的数据
            ::memcpy((char*)buf + bufPos, currentNode_->data + nodePos, size);
            nodePos += size;
            break;
        } else if (nodeBytesLeft == size) { //刚好读完
            ::memcpy((char*)buf + bufPos, currentNode_->data + nodePos, size);
            currentNode_ = static_cast<Node*>(currentNode_->next);
            nodePos = 0;
            break;
        }else {
            ::memcpy((char*)buf + bufPos, currentNode_->data + nodePos, nodeBytesLeft);
            bufPos += nodeBytesLeft;
            size -= nodeBytesLeft;
            currentNode_ = static_cast<Node*>(currentNode_->next); //在下一个节点继续读取数据
            nodeBytesLeft = blockSize_;
            nodePos = 0; 
        }
    }

    currNodePosition_ = nodePos;
}

void ByteArray::readNonMove(void* buf, size_t size) const {

}

void ByteArray::seek(size_t n) {
    return;
}

bool ByteArray::writeToFile(StringArg filename) const {
    file_util::FileAppender fileAppender(filename);
    if(!fileAppender.isOpen()) {
        NEMO_LOG_ERROR(systemLogger) << "writeToFile name=" << filename
            << " error , errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    size_t readSize = getReadableBytes();
    size_t pos = currNodePosition_;
    Node* nodeToWrite = currentNode_;

    while(readSize > 0) {
        size_t len = std::min(static_cast<size_t>(blockSize_), readSize) - pos;
        fileAppender.append(nodeToWrite->data + pos, len);
        nodeToWrite = static_cast<Node*>(nodeToWrite->next);
        pos = (pos + len) % blockSize_;;
        readSize -= len;
    }

    return true;
}

bool ByteArray::readFromFile(StringArg filename) {
    FILE* file = ::fopen(filename.data(), "re");
    
    if(!file) {
        NEMO_LOG_ERROR(systemLogger) << "readFromFile name=" << filename
            << " error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }

    Buffer::UniquePtr buff = std::make_unique<Buffer>(kBytesPerPage, true);
    while (true) {
        int n = ::fread_unlocked(buff->data(), 1, buff->size(), file);
        if (feof(file) || 0 == n) {
            break;
        }
    }
    fclose(file);

    return true;
}

String ByteArray::toString() const {
    String str;
    str.resize(getReadableBytes());
    if(str.empty()) {
        return str;
    }
    readNonMove(&str[0], str.size());
    return str;
}

String ByteArray::toHexString() const {
    String str = toString();
    std::stringstream ss;

    for(size_t i = 0; i < str.size(); ++i) {
        if(i > 0 && i % 32 == 0) {
            ss << '\n';
        }
        ss << std::setw(2) << std::setfill('0') << std::hex
           << (int)(uint8_t)str[i] << " ";
    }

    return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
    size_t readableBytes = getReadableBytes();
    len = std::min(len, readableBytes);
    if(0 == len) {
        return 0;
    }

    uint64_t size = len;

    size_t nodePos = currNodePosition_;
    size_t nodeBytesLeft = blockSize_ - currNodePosition_;
    struct iovec iov;
    Node* curr = currentNode_;

    while(len > 0) {
        if(nodeBytesLeft >= len) {
            iov.iov_base = curr->data + nodePos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = curr->data + nodePos;
            iov.iov_len = nodeBytesLeft;
            len -= nodeBytesLeft;
            curr = static_cast<Node*>(curr->next);
            nodeBytesLeft = blockSize_;
            nodePos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const {
    return 0;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
    if(0 == len) {
        return 0;
    }
    ensureWritableSize(len);
    uint64_t size = len;

    size_t nodePos = currNodePosition_;
    size_t nodeBytesLeft = blockSize_ - currNodePosition_;
    struct iovec iov;
    Node* curr = currentNode_;
    while(len > 0) {
        if(nodeBytesLeft >= len) {
            iov.iov_base = curr->data + nodePos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = curr->data + nodePos;
            iov.iov_len = nodeBytesLeft;
            len -= nodeBytesLeft;
            curr = static_cast<Node*>(curr->next);
            nodeBytesLeft = blockSize_;
            nodePos = 0;
        }
        buffers.push_back(iov);
    }
    return size;
}

} // namespace nemo