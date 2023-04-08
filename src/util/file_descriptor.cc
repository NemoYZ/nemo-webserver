#include "util/file_descriptor.h"

#include <fcntl.h>

#include "log/log.h"
#include "util/util.h"

namespace nemo {
namespace file_util {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

const char* FdContext::FdType2String(FdType type) {
#define CASE(name) \
        case FdType::name: \
            return #name

    switch (type) {
        CASE(Socket);
        CASE(Pipe);
        CASE(Plain);
    default:
        return "Unknown";
    }

#undef CASE
    return "Unknown";
}

FdContext::FdType FdContext::String2FdType(const StringArg& str) {
#define IF(type, v) \
    do {            \
        if (strncasecmp(str.data(), #v, str.size()) == 0) { \
            return FdType::type; \
        } \
    } while(0)

    IF(Socket, socket);
    IF(Pipe, pipe);
    IF(Plain, plain);
    
    return FdType::Unknown;
}

FdContext::FdContext(int fd, FdType fdType, bool isNonBlocking, const net::SocketAttribute& sockAttr) :
    ReactorElement(fd),
    sendTimeout_(0),
    recvTimeout_(0),
    socketAttr_(sockAttr),
    tcpConnectTimeout_(0),
    type_(fdType),
    isNonBlocking_(isNonBlocking) {
}

bool FdContext::isTcpSocket() const { 
    return isSocket() && 
        socketAttr_.type == SOCK_STREAM && 
        (socketAttr_.family == AF_INET || socketAttr_.family == AF_INET6);
}

bool FdContext::setNonBlocking(bool isNonBlocking) {
    int flags = InvokeSlowSystemCall(::fcntl, fd_, F_GETFL, 0);
    bool old = flags & O_NONBLOCK;
    if (isNonBlocking == old) { 
        return old;
    }

    InvokeSlowSystemCall(::fcntl, fd_, F_SETFL,
            isNonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
    onSetNonBlocking(isNonBlocking);
    return true;
}

long FdContext::getSocketTimeoutMicroSeconds(int timeoutType) {
    switch (timeoutType) {
        case SO_RCVTIMEO:
            return recvTimeout_;
        case SO_SNDTIMEO:
            return sendTimeout_;
    }

    return 0;
}

void FdContext::onSetSocketTimeout(int timeoutType, int microseconds) {
    switch (timeoutType) {
        case SO_RCVTIMEO:
            recvTimeout_ = microseconds;
            break;
        case SO_SNDTIMEO:
            sendTimeout_ = microseconds;
            break;
    }
}

FdContext::UniquePtr FdContext::clone(int newFd) {
    FdContext::UniquePtr newFdContext = std::make_unique<FdContext>(newFd, type_, isNonBlocking_, socketAttr_);
    newFdContext->tcpConnectTimeout_ = tcpConnectTimeout_;
    newFdContext->recvTimeout_ = recvTimeout_;
    newFdContext->sendTimeout_ = sendTimeout_;
    return newFdContext;
}

void FdContext::onClose() {
    ReactorElement::onClose();
}

bool FdManager::add(FdContext::UniquePtr&& fdCtx) {
    if (fdCtx) {
        int fd = fdCtx->fd_;
        std::lock_guard<std::mutex> lockGuard(mutex_);
        fdContexts_.emplace(fd, std::move(fdCtx));
    }

    return false;
}

FdContext* FdManager::get(int fd) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    auto iter = fdContexts_.find(fd);
    return iter == fdContexts_.end() ? nullptr : iter->second.get();
}

void FdManager::erase(int fd) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    fdContexts_.erase(fd);
}

void FdManager::erase(FdContext* fdCtx) {
    if (fdCtx) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        fdContexts_.erase(fdCtx->fd_);
    }
}

} // namespace file_util
} // namespace nemo