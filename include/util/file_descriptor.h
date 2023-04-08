#pragma once

#include <sys/socket.h>

#include <memory>
#include <unordered_map>
#include <mutex>

#include "net/io/reactor_element.h"
#include "net/socket_attribute.h"
#include "common/types.h"
#include "common/lexical_cast.h"
#include "common/singleton.h"
#include "common/noncopyable.h"

namespace nemo {
namespace file_util {

class FdManager;

class FdContext : public net::io::ReactorElement {
friend class FdManager;
public:
    typedef std::shared_ptr<FdContext> SharedPtr;
    typedef std::unique_ptr<FdContext> UniquePtr;

public:
    enum FdType : int8_t {
        Socket,
        Pipe,
        Plain,
        Unknown,
    };

    static const char* FdType2String(FdType type);
    static FdType String2FdType(const StringArg& str);

public:
    explicit FdContext(int fd, FdType fdType, bool isNonBlocking, const net::SocketAttribute& sockAttr);

public:
    bool isSocket() const { return type_ == FdType::Socket; }
    bool isTcpSocket() const;
    bool isNonBlocking() const { return isNonBlocking_; }
    bool setNonBlocking(bool isNonBlocking);
    FdType getType() const { return type_; }
    void setTcpConnectTimeout(int milliseconds) { tcpConnectTimeout_ = milliseconds; }
    int getTcpConnectTimeout() { return tcpConnectTimeout_; }
    long getSocketTimeoutMicroSeconds(int timeoutType);
    net::SocketAttribute getSocketAttribute() const { return socketAttr_; }
    void onSetNonBlocking(bool isNonBlocking) { isNonBlocking_ = isNonBlocking; }
    void onSetSocketTimeout(int timeoutType, int microseconds);
    FdContext::UniquePtr clone(int newFd);
    void onClose();

private:
    long sendTimeout_;
    long recvTimeout_;
    net::SocketAttribute socketAttr_;
    int tcpConnectTimeout_;
    FdType type_;
    bool isNonBlocking_;
};

class FdManager : public Singleton<FdManager> {
public:
    FdManager(Token token) {}

public:
    bool add(FdContext::UniquePtr&& fdCtx);
    FdContext* get(int fd);
    void erase(int fd);
    void erase(FdContext* fdCtx);
    
private:
    typedef std::unordered_map<int, FdContext::UniquePtr> FdContextMap;

private:
    std::mutex mutex_;
    FdContextMap fdContexts_;
};

} // namespace file_util

template<>
inline const char*
LexicalCast<const char*, file_util::FdContext::FdType>(
        const file_util::FdContext::FdType& type) {
    return file_util::FdContext::FdType2String(type);
}

template<>
inline file_util::FdContext::FdType
LexicalCast<file_util::FdContext::FdType, StringArg>(const StringArg& str) {
    return file_util::FdContext::String2FdType(str);
}

} // nemo