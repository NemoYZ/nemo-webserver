#include "container/buffer.h"

#include <malloc.h>
#include <string.h>

#include <algorithm>

namespace nemo {

void Buffer::ensureCapacity(size_t n) {
    size_t sz = size();
    char* newBuff = static_cast<char*>(::malloc(sizeof(char) * n));
    ::memcpy(newBuff, buff_, sz);
    ::free(buff_);
    buff_ = newBuff;
    curr_ = buff_ + sz;
    endOfStorage_ = buff_ + n;
}

Buffer::Buffer(size_t n, bool fxied) :
    buff_(static_cast<char*>(::malloc(sizeof(char) * n))),
    curr_(buff_),
    endOfStorage_(buff_ + sizeof(char) * n),
    fixed_(fxied) {
}

Buffer::Buffer(const Buffer& other) :
    buff_(static_cast<char*>(::malloc(other.capacity()))),
    curr_(buff_ + other.size()),
    endOfStorage_(buff_ + other.capacity()),
    fixed_(other.fixed_) {
    ::memcpy(buff_, other.buff_, other.size());
}

Buffer::Buffer(Buffer&& other) noexcept :
    buff_(other.buff_),
    curr_(other.curr_),
    endOfStorage_(other.endOfStorage_),
    fixed_(other.fixed_) {
    other.buff_ = other.curr_ = other.endOfStorage_ = nullptr;
}

Buffer::~Buffer() noexcept {
    ::free(buff_);
}

Buffer& Buffer::operator=(const Buffer& other) {
    if (this != &other) {
        buff_ = static_cast<char*>(::malloc(other.capacity()));
        curr_ = buff_ + other.size();
        endOfStorage_ = buff_ + other.capacity();
        fixed_ = other.fixed_;
        ::memcpy(buff_, other.buff_, other.size());
    }

    return *this;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        buff_ = other.buff_;
        curr_ = other.curr_;
        endOfStorage_ = other.endOfStorage_;
        fixed_ = other.fixed_;
        other.buff_ = other.curr_ = other.endOfStorage_ = nullptr;
    }

    return *this;
}

void Buffer::resize(size_t n) {
    if (n < capacity()) {
        curr_ = buff_ + n;
    } else {
        ensureCapacity(n);
        curr_ = endOfStorage_;
    }
}

size_t Buffer::append(const void* buff, size_t len) {
    if (avail() < len) {
        if (fixed_) {
            return 0;
        } else {
            size_t newSize = std::max(size() * 2, len + len / 2 + size());
            ensureCapacity(newSize);
        }
    }

    ::memcpy(curr_, buff, len);
    curr_ += len;
    
    return len;
}

size_t Buffer::appendPossible(const void* buff, size_t len) {
    if (avail() < len) {
        if (fixed_) {
            len = avail() - 1;
        } else {
            size_t newSize = std::max(size() * 2, len + len / 2 + size());
            ensureCapacity(newSize);
        }
    }

    ::memcpy(curr_, buff, len);
    curr_ += len;
    return len;
}

} //namespace nemo