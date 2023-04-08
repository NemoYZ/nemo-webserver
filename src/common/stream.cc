#include "common/stream.h"

namespace nemo {

int Stream::readFixSize(void* buffer, size_t length) {
    size_t offset = 0;
    int64_t left = length;
    while(left > 0) {
        int64_t len = read(static_cast<char*>(buffer) + offset, left);
        if(len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }
    return length;
}

int Stream::readFixSize(ByteArray* bytearray, size_t length) {
    int64_t left = length;
    while(left > 0) {
        int64_t len = read(bytearray, left);
        if(len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

int Stream::writeFixSize(const void* buffer, size_t length) {
    size_t offset = 0;
    int64_t left = length;
    while(left > 0) {
        int64_t len = write(static_cast<const char*>(buffer) + offset, left);
        if(len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }
    return length;

}

int Stream::writeFixSize(ByteArray* bytearray, size_t length) {
    int64_t left = length;
    while(left > 0) {
        int64_t len = write(bytearray, left);
        if(len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

} // namespace nemo