#pragma once

#include <memory>

#include "common/types.h"
#include "util/util.h"

namespace nemo {

/**
 * @brief 开辟堆内存的缓冲区实现
 * 
 */
class Buffer {
public:
    typedef std::shared_ptr<Buffer> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Buffer> UniquePtr; ///< 智能指针定义

public:
    explicit Buffer(size_t n = 16, bool fixed = false);
    Buffer(const Buffer& other);
    Buffer(Buffer&& other) noexcept;
    ~Buffer() noexcept;

public:
    Buffer& operator=(const Buffer& other);
    Buffer& operator=(Buffer&& other) noexcept;

public:
    void resize(size_t n);
    void clear() noexcept { curr_ = buff_; }
    void bzero() noexcept { MemoryZero(buff_, capacity()); }

    void seek(intptr_t stride) noexcept { curr_ += stride; }
    size_t append(const void* /*restrict*/ buff, size_t len);
    size_t appendPossible(const void* /*restrict*/ buff, size_t len);
    size_t size() const noexcept { return curr_ - buff_; }
    size_t length() const noexcept { return curr_ - buff_; }
    size_t avail() const noexcept { return endOfStorage_ - curr_; }
    size_t capacity() const noexcept { return endOfStorage_ - buff_; }
    char* current() const noexcept { return curr_; }
    char* data() const noexcept { return buff_; }
    char* begin() const noexcept { return buff_; }
    char* end() const noexcept { return curr_; }
    bool filled() const noexcept { return fixed_ ? curr_ == endOfStorage_ : false; }
    bool empty() const noexcept { return curr_ == buff_; }
    String toString() const { return String(buff_, size()); }

private:
    void ensureCapacity(size_t n);

private:
    char* buff_;
    char* curr_;
    char* endOfStorage_;
    bool fixed_;
};

/*Do we need this?
bool operator==(const Buffer& lhs, const Buffer& rhs);
bool operator!=(const Buffer& lhs, const Buffer& rhs);
bool operator<(const Buffer& lhs, const Buffer& rhs);
bool operator>(const Buffer& lhs, const Buffer& rhs);
bool operator<=(const Buffer& lhs, const Buffer& rhs);
bool operator>=(const Buffer& lhs, const Buffer& rhs);
*/

} //namespace nemo