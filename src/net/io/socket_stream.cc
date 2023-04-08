#include "net/io/socket_stream.h"

#include "log/log.h"

namespace nemo {
namespace net {
namespace io {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

SocketStream::SocketStream(Socket* sock) :
    socket_(sock),
    owner_(false) {
}

SocketStream::SocketStream(Socket::UniquePtr&& sock) :
    socket_(std::move(sock)),
    owner_(false) {
}

SocketStream::~SocketStream() {
    if (!owner_) {
        socket_.release();
    }
}

int SocketStream::read(void* buffer, size_t length) {
    if(!isConnect()) {
        NEMO_LOG_WARN(systemLogger) << "Not connect when read. socket: "
                                << (socket_ ? socket_->toString() : "null");
        return -1;
    }
    return socket_->recv(buffer, length);
}

int SocketStream::read(ByteArray* bytearray, size_t length) {
    if(!isConnect()) {
        NEMO_LOG_WARN(systemLogger) << "Not connect when read. socket: "
                                << (socket_ ? socket_->toString() : "null");
        return -1;
    }
    std::vector<iovec> iovs;
    bytearray->getWriteBuffers(iovs, length);
    int rt = socket_->recv(&iovs[0], iovs.size());
    if(rt > 0) {
        bytearray->seek(bytearray->getPosition() + rt);
    }
    return rt;
}

int SocketStream::write(const void* buffer, size_t length) {
    if(!isConnect()) {
        NEMO_LOG_WARN(systemLogger) << "Not connect when write. socket: "
                                << (socket_ ? socket_->toString() : "null");
        return -1;
    }
    return socket_->send(buffer, length);
}

int SocketStream::write(ByteArray* bytearrray, size_t length) {
    if(!isConnect()) {
        NEMO_LOG_WARN(systemLogger) << "Not connect when write. socket: "
                                << (socket_ ? socket_->toString() : "null");
        return -1;
    }
    std::vector<iovec> iovs;
    bytearrray->getReadBuffers(iovs, length);
    int rt = socket_->send(&iovs[0], iovs.size());
    if(rt > 0) {
        bytearrray->seek(bytearrray->getPosition() + rt);
    }
    return rt;
}

void SocketStream::close() {
    if(socket_) {
        socket_->close();
    }
}

Address* SocketStream::getRemoteAddress() {
    if(socket_) {
        return socket_->getRemoteAddress();
    }
    return nullptr;
}

Address* SocketStream::getLocalAddress() {
    if(socket_) {
        return socket_->getLocalAddress();
    }
    return nullptr;
}

String SocketStream::getRemoteAddressString() {
    auto addr = getRemoteAddress();
    if(addr) {
        return addr->toString();
    }
    return "";
}

String SocketStream::getLocalAddressString() {
    auto addr = getLocalAddress();
    if(addr) {
        return addr->toString();
    }
    return "";
}

} // namespace io
} // namespace net
} // namespace nemo