/**
 * @file byte_array.h
 * @brief 可以参考deque，但这里我们用链表，链表每个节点一个页
 */

#pragma once

#include <sys/socket.h>

#include <stdint.h> // C++17 byte
#include <memory>
#include <vector>

#include "system/parameter.h"
#include "common/types.h"
#include "container/list.h"

namespace nemo {

typedef uint8_t Byte; ///< 字节类型定义
static_assert((sizeof(Byte) == sizeof(std::byte)) && (sizeof(Byte) == 1), "not align");

struct ByteArrayListNode : public ListNodeBase {
    Byte data[0];
};

class ByteArray {
public:
    typedef std::shared_ptr<ByteArray> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<ByteArray> UniquePtr; ///< 智能指针定义

public:
    constexpr static int kDefaultBytePerBlock = 
        kBytesPerPage - sizeof(ByteArrayListNode);

public:
    /**
     * @brief 使用指定长度的内存块构造ByteArray
     * @param[in] blockSize 内存块大小
     */
    ByteArray(const int blockSize = kDefaultBytePerBlock);

    /**
     * @brief 析构函数
     */
    ~ByteArray();

    /**
     * @brief 写入固定长度int8_t类型的数据
     * @post position_ += sizeof(value)
     *       如果position_ > size_ 则 size_ = position_
     */
    void writeFixed(int8_t value);

    void writeFixed(uint8_t value);

    void writeFixed(int16_t value);

    void writeFixed(uint16_t value);

    void writeFixed(int32_t value);

    void writeFixed(uint32_t value);

    void writeFixed(int64_t value);

    void writeFixed(uint64_t value);

    void write(int32_t value);

    void write(uint32_t value);

    void write(int64_t value);

    void write(uint64_t value);

    void write(float value);

    void write(double value);

    void writeFixedString16(StringArg value);

    /**
     * @brief 写入std::string类型的数据,用uint32_t作为长度类型
     * @post mPosition += sizeof(uint32_t) + value.size()
     *       如果mPosition > mSize 则 mSize = mPosition
     */
    void writeFixedString32(StringArg value);

    /**
     * @brief 写入std::string类型的数据,用uint64_t作为长度类型
     * @post mPosition += sizeof(uint64_t) + value.size()
     *       如果mPosition > mSize 则 mSize = mPosition
     */
    void writeFixedString64(StringArg value);

    /**
     * @brief 写入std::string类型的数据,用无符号Varint64作为长度类型
     * @post mPosition += Varint64长度 + value.size()
     *       如果mPosition > mSize 则 mSize = mPosition
     */
    void writeVariantStringint(StringArg value);

    /**
     * @brief 写入std::string类型的数据,无长度
     * @post mPosition += value.size()
     *       如果mPosition > mSize 则 mSize = mPosition
     */
    void writeString(StringArg value);

    /**
     * @brief 读取int8_t类型的数据
     * @pre getReadableSize() >= sizeof(int8_t)
     * @post position_ += sizeof(int8_t);
     * @exception 如果getReadableSize() < sizeof(int8_t) 抛出 std::out_of_range
     */
    int8_t readFixedInt8();

    uint8_t readFixedUint8();

    int16_t readFixedInt16();

    uint16_t readFixedUint16();

    int32_t readFixedInt32();

    uint32_t readFixedUint32();

    int64_t readFixedInt64();

    uint64_t readFixedUint64();

    int32_t readInt32();

    uint32_t readUint32();

    int64_t readInt64();

    uint64_t readUint64();

    float readFloat();

    double readDouble();

    /**
     * @brief 读取String类型的数据,用uint16_t作为长度
     * @pre getReadableSize() >= sizeof(uint16_t) + size
     * @post position_ += sizeof(uint16_t) + size;
     * @exception getReadableSize() < sizeof(uint16_t) + size 抛出 std::out_of_range
     */
    String readFixedString16();

    String readFixedString32();

    String readFixedString64();

    /**
     * @brief 读取std::string类型的数据,用无符号Varint64作为长度
     * @pre getReadableSize() >= 无符号Varint64实际大小 + size
     * @post mPosition += 无符号Varint64实际大小 + size;
     * @exception getReadableSize() < 无符号Varint64实际大小 + size 抛出 std::out_of_range
     */
    String readVariantStringint();

    /**
     * @brief 清空ByteArray
     * @post position_ = 0, size_ = 0
     */
    void clear();

    /**
     * @brief 写入size长度的数据
     * @param[in] buf 内存缓存指针
     * @param[in] size 数据大小
     * @post position_ += size, 如果position_ > size_ 则 mSize = mPosition
     */
    void write(const void* buf, size_t size);

    /**
     * @brief 读取size长度的数据
     * @param[out] buf 内存缓存指针
     * @param[in] size 数据大小
     * @post position_ += size, 如果position_ > size_ 则 size_ = position_
     * @exception 如果getReadableSize() < size 则抛出 std::out_of_range
     */
    void read(void* buf, size_t size);

    void readNonMove(void* buf, size_t size) const;

    /**
     * @brief 返回ByteArray当前位置
     */
    size_t getPosition() const { return currNodePosition_; }

    /**
     * @brief 设置ByteArray当前位置
     * @post 如果mPosition > mSize 则 mSize = mPosition
     * @exception 如果mPosition > mCapacity 则抛出 std::out_of_range
     */
    void seek(size_t n);

    /**
     * @brief 把ByteArray的数据写入到文件中
     * @param[in] name 文件名
     */
    bool writeToFile(StringArg filename) const;

    /**
     * @brief 从文件中读取数据
     * @param[in] name 文件名
     */
    bool readFromFile(StringArg filename);

    /**
     * @brief 返回内存块的大小
     */
    int getBlockSize() const { return blockSize_; }

    /**
     * @brief 返回可读取数据大小
     */
    size_t getReadableBytes() const { 
        return (nodeCount_ - currNodeIndex_ - 1) * blockSize_ + 
            lastNodeBytesUsed_; 
    }

    std::endian getEndian() const { return endian_; }

    /**
     * @brief 设置是否为小端
     */
    void setEndian(std::endian e) { endian_ = e; }

    /**
     * @brief 将ByteArray里面的数据[mPosition, mSize)转成std::string
     */
    String toString() const;

    /**
     * @brief 将ByteArray里面的数据[mPosition, mSize)转成16进制的std::string(格式:FF FF FF)
     */
    String toHexString() const;

    /**
     * @brief 获取可读取的缓存,保存成iovec数组
     * @param[out] buffers 保存可读取数据的iovec数组
     * @param[in] len 读取数据的长度,如果len > getReadableSize() 则 len = getReadableSize()
     * @return 返回实际数据的长度
     */
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;

    /**
     * @brief 获取可读取的缓存,保存成iovec数组,从position位置开始
     * @param[out] buffers 保存可读取数据的iovec数组
     * @param[in] len 读取数据的长度,如果len > getReadableSize() 则 len = getReadableSize()
     * @param[in] position 读取数据的位置
     * @return 返回实际数据的长度
     */
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;

    /**
     * @brief 获取可写入的缓存,保存成iovec数组
     * @param[out] buffers 保存可写入的内存的iovec数组
     * @param[in] len 写入的长度
     * @return 返回实际的长度
     * @post 如果(mPosition + len) > mCapacity 则 mCapacity扩容N个节点以容纳len长度
     */
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    size_t getLocalSize() const { return 0; }

    /**
     * @brief 返回数据的长度
     */
    size_t getTotalBytes() const {
        return (nodeCount_ - 1) * blockSize_ + 
            lastNodeBytesUsed_;
    }

private:
    typedef ByteArrayListNode Node;
    
private:
    Node* allocateNode();
    void deallocateNode(Node* node);
    void ensureWritableSize(size_t size);

private:
    Node* firstNode_;         ///< 第一个内存块指针
    Node* lastNode_;          ///< 最后一个节点
    Node* currentNode_;       ///< 当前操作的内存块指针
    size_t nodeCount_;        ///< 总的节点个数
    size_t currNodeIndex_;    ///< 当前节点在链表中是第几个节点(从0开始)
    int currNodePosition_;    ///< 当前节点所操作位置
    int lastNodeBytesUsed_;   ///< 最后一个节点已用字节数
    const int blockSize_;     ///< 内存块的大小
    std::endian endian_;      ///< 字节端
};

} // namespace nemo