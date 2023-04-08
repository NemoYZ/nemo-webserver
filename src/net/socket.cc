#include "net/socket.h"

#include <netinet/tcp.h>

#include "net/io/hook.h"
#include "log/log.h"
#include "util/file_descriptor.h"
#include "common/macro.h"

namespace nemo {
namespace net {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

void Socket::initSock() {
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if(SOCK_STREAM == sockAttr_.type) {
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

void Socket::newSock() {
    sockFd_ = ::socket(sockAttr_.family, sockAttr_.type, sockAttr_.protocol);
    if(sockFd_ != -1) {
        initSock();
    } else {
        NEMO_LOG_ERROR(systemLogger) << "socket(" << sockAttr_.family
            << ", " << sockAttr_.type << ", " << sockAttr_.protocol << ") errno="
            << errno << " errstr=" << strerror(errno);
    }
}

bool Socket::init(int sock) {
    file_util::FdContext* fdCtx = file_util::FdManager::GetInstance().get(sock);
    if(fdCtx && file_util::FdContext::FdType::Socket == fdCtx->getType()) {
        sockFd_ = sock;
        isConnect_ = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

Socket::UniquePtr Socket::CreateTcp(const Address* address) {
    return std::make_unique<Socket>(address->getFamily(), SocketAttribute::Type::Tcp, 0);
}

Socket::UniquePtr Socket::CreateUdp(const Address* address) {
    Socket::UniquePtr sock = std::make_unique<Socket>(address->getFamily(), SocketAttribute::Type::Udp, 0);
    sock->newSock();
    sock->isConnect_ = true;
    return sock;
}

Socket::UniquePtr Socket::CreateTcpSocket() {
    return std::make_unique<Socket>(SocketAttribute::Family::Ipv4, SocketAttribute::Type::Tcp, 0);
}

Socket::UniquePtr Socket::CreateUdpSocket() {
    Socket::UniquePtr sock = std::make_unique<Socket>(SocketAttribute::Family::Ipv4, SocketAttribute::Type::Udp, 0);
    sock->newSock();
    sock->isConnect_ = true;
    return sock;
}

Socket::UniquePtr Socket::CreateTcpSocket6() {
    return std::make_unique<Socket>(SocketAttribute::Family::Ipv6, SocketAttribute::Type::Tcp, 0);
}

Socket::UniquePtr Socket::CreateUdpSocket6() {
    Socket::UniquePtr sock = std::make_unique<Socket>(SocketAttribute::Family::Ipv6, SocketAttribute::Type::Udp, 0);
    sock->newSock();
    sock->isConnect_ = true;
    return sock;
}

Socket::UniquePtr Socket::CreateUnixTcpSocket() {
    return std::make_unique<Socket>(SocketAttribute::Family::Unix, SocketAttribute::Type::Tcp, 0);
}

Socket::UniquePtr Socket::CreateUnixUdpSocket() {
    return std::make_unique<Socket>(SocketAttribute::Family::Unix, SocketAttribute::Type::Udp, 0);
}

Socket::Socket(int family, int type, int protocol) :
    sockAttr_(family, type, protocol),
    sockFd_(-1),
    isConnect_(false) {
}

Socket::Socket(const SocketAttribute& attribute) :
    sockAttr_(attribute),
    sockFd_(-1),
    isConnect_(false) {
}

Socket::~Socket() {
    close();
}

int64_t Socket::getSendTimeout() {
    file_util::FdContext* fdCtx = file_util::FdManager::GetInstance().get(sockFd_);
    if(fdCtx) {
        return fdCtx->getSocketTimeoutMicroSeconds(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t timeoutMillionSeconds) {
    struct timeval tv{int(timeoutMillionSeconds / 1000), 
        int(timeoutMillionSeconds % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
    file_util::FdContext* fdCtx = file_util::FdManager::GetInstance().get(sockFd_);
    if(fdCtx) {
        return fdCtx->getSocketTimeoutMicroSeconds(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t timeoutMillionSeconds) {
    struct timeval tv{int(timeoutMillionSeconds / 1000), 
        int(timeoutMillionSeconds % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len) {
    int ret = ::getsockopt(sockFd_, level, option, result, (socklen_t*)len);
    if(NEMO_UNLIKELY(ret)) {
        NEMO_LOG_ERROR(systemLogger) << "getOption sock=" << sockFd_
            << " level=" << level << " option=" << option
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len) {
    int ret = ::setsockopt(sockFd_, level, option, result, (socklen_t)len);
    if(NEMO_UNLIKELY(ret)) {
        NEMO_LOG_ERROR(systemLogger) << "setOption sock=" << sockFd_
            << " level=" << level << " option=" << option
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

Socket::UniquePtr Socket::accept() {
    Socket::UniquePtr sock = std::make_unique<Socket>(sockAttr_);
    int newSocktFd = ::accept(sockFd_, nullptr, nullptr);
    if(-1 == newSocktFd) {
        NEMO_LOG_ERROR(systemLogger) << "accept(" << sockFd_ << ") errno="
            << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    if(!sock->init(newSocktFd)) {
        return nullptr;
    }
    return sock;
}

bool Socket::bind(const Address* addr) {
    if(!isValid()) {
        newSock();
        if(!isValid()) {
            return false;
        }
    }

    if(addr->getFamily() != sockAttr_.family) {
        NEMO_LOG_ERROR(systemLogger) << "bind sock.family("
            << sockAttr_.family << ") address.family(" << addr->getFamily()
            << ") not equal, address=" << addr->toString();
        return false;
    }

    if(::bind(sockFd_, addr->getAddr(), addr->getAddrLen())) {
        NEMO_LOG_ERROR(systemLogger) << "bind error errrno=" << errno
            << " errstr=" << strerror(errno);
        return false;
    }

    getLocalAddress();	//服务器端不需要remoteAddress
    return true;
}


bool Socket::connect(const Address* addr) {
    if(!isValid()) {
        newSock();
        if(!isValid()) {
            return false;
        }
    }

    if(addr->getFamily() != sockAttr_.family) {
        NEMO_LOG_ERROR(systemLogger) << "connect sock.family("
            << sockAttr_.family << ") address.family(" 
            << remoteAddress_->getFamily()
            << ") not equal, address=" << addr->toString();
        return false;
    }
    
    if(::connect(sockFd_, addr->getAddr(), addr->getAddrLen())) {
        NEMO_LOG_ERROR(systemLogger) << "sock=" << sockFd_ 
            << " connect(" << addr->toString()
            << ") error errno=" << errno 
            << " errstr=" << strerror(errno);
        close();
        return false;
    }
    
    
    isConnect_ = true;

    // 客户端
    // remote address和local address都要
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog) {
    if(!isValid()) {
        NEMO_LOG_ERROR(systemLogger) << "listen error sock=-1";
        return false;
    }
    if(::listen(sockFd_, backlog)) {
        NEMO_LOG_ERROR(systemLogger) << "listen error errno=" << errno
            << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close() {
    if(!isConnect_ && -1 == sockFd_) {
        return true;
    }
    isConnect_ = false;
    if(sockFd_ != -1) {
        int result = ::close(sockFd_);
        if (NEMO_UNLIKELY(result)) {
            NEMO_LOG_ERROR(systemLogger) << "close error, errno=" << errno
                << " errstr=" << strerror(errno);
            return false;
        }
        sockFd_ = -1;
    }
    return true;
}

int Socket::send(const void* buffer, size_t length, int flags) {
    if(isConnect()) {
        return ::send(sockFd_, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
    if(isConnect()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::sendmsg(sockFd_, &msg, flags);
    }
    return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address* to, int flags) {
    if(isConnect()) {
        return ::sendto(sockFd_, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address* to, int flags) {
    if(isConnect()) {
        msghdr msg;
        MemoryZero(&msg, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = (void*)to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(sockFd_, &msg, flags);
    }
    return -1;
}

int Socket::recv(void* buffer, size_t length, int flags) {
    if(isConnect()) {
        return ::recv(sockFd_, buffer, length, flags);
    }
    return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags) {
    if(isConnect()) {
        msghdr msg;
        MemoryZero(&msg, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(sockFd_, &msg, flags);
    }
    return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address* from, int flags) {
    if(isConnect()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(sockFd_, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address* from, int flags) {
    if(isConnect()) {
        msghdr msg;
        MemoryZero(&msg, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(sockFd_, &msg, flags);
    }
    return -1;
}

Address* Socket::getRemoteAddress() {
    if(remoteAddress_) {
        return remoteAddress_.get();
    }

    switch(sockAttr_.family) {
        case AF_INET:
            remoteAddress_.reset(new Ipv4Address);
            break;
        case AF_INET6:
            remoteAddress_.reset(new Ipv6Address);
            break;
        case AF_UNIX:
            remoteAddress_.reset(new UnixAddress);
            break;
        default:
            remoteAddress_.reset(new UnknownAddress(sockAttr_.family));
            break;
    }

    socklen_t addrlen = remoteAddress_->getAddrLen();
	//getpeername函数用于获取与某个套接字关联的外地协议地址
    int result = ::getpeername(sockFd_, remoteAddress_->getAddr(), &addrlen);
    if(NEMO_UNLIKELY(result)) {
        NEMO_LOG_DEBUG(systemLogger) << "getpeername error sock=" << sockFd_
            << " errno=" << errno << " errstr=" << strerror(errno);
        remoteAddress_.reset();
    } else {
        if(AF_UNIX == sockAttr_.family) {
            UnixAddress* unixAddress = down_cast<UnixAddress*>(remoteAddress_.get());
            unixAddress->setAddrLen(addrlen);
        }
    }
    
    return remoteAddress_.get();
}

Address* Socket::getLocalAddress() {
    if(localAddress_) {
        return localAddress_.get();
    }

    switch(sockAttr_.family) {
        case AF_INET:
            localAddress_.reset(new Ipv4Address());
            break;
        case AF_INET6:
            localAddress_.reset(new Ipv6Address());
            break;
        case AF_UNIX:
            localAddress_.reset(new UnixAddress());
            break;
        default:
            localAddress_.reset(new UnknownAddress(sockAttr_.family));
            break;
    }

    socklen_t addrlen = localAddress_->getAddrLen();
    int result = ::getsockname(sockFd_, localAddress_->getAddr(), &addrlen);
    if(NEMO_UNLIKELY(result)) {
        NEMO_LOG_ERROR(systemLogger) << "getsockname error sock=" << sockFd_
            << " errno=" << errno << " errstr=" << strerror(errno);
        localAddress_.reset();
    } else {
        if(AF_UNIX == sockAttr_.family) {
            UnixAddress* unixAddress = down_cast<UnixAddress*>(localAddress_.get());
            unixAddress->setAddrLen(addrlen);
        }
    }

    return localAddress_.get();
}

int Socket::getError() {
    int error = 0;
    socklen_t len = sizeof(error);
    if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
        error = errno;
    }
    return error;
}

std::ostream& Socket::dump(std::ostream& os) const {
    os << "[Socket sock=" << sockFd_
       << " is_connected=" << isConnect_
       << " family=" << sockAttr_.family
       << " type=" << sockAttr_.type
       << " protocol=" << sockAttr_.protocol;
    if(localAddress_) {
        os << " local_address=" << localAddress_->toString();
    }
    if(remoteAddress_) {
        os << " remote_address=" << remoteAddress_->toString();
    }
    os << "]";
    return os;
}

std::string Socket::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

namespace {

struct SslInit {
    SslInit() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    }
};

static SslInit sslInit;

}

bool SecureSocket::init(int sock) {
    bool result = Socket::init(sock);
    if(result) {
        ssl_.reset(SSL_new(sslCtx_.get()));
        ::SSL_set_fd(ssl_.get(), sockFd_);
        result = (SSL_accept(ssl_.get()) == 1);
    }
    return result;
}

SecureSocket::UniquePtr SecureSocket::CreateTcp(Address* address) {
    return std::make_unique<SecureSocket>(address->getFamily(), 
        SocketAttribute::Type::Tcp, 0);
}

SecureSocket::UniquePtr SecureSocket::CreateTcpSocket(Address* address) {
    return std::make_unique<SecureSocket>(SocketAttribute::Family::Ipv4, 
        SocketAttribute::Type::Tcp, 0);
}

SecureSocket::UniquePtr SecureSocket::CreateTcpSocket6(Address* address) {
    return std::make_unique<SecureSocket>(SocketAttribute::Family::Ipv6, 
        SocketAttribute::Type::Tcp, 0);
}

SecureSocket::SecureSocket(int family, int type, int protocol) :
    Socket(family, type, protocol) {
}

SecureSocket::SecureSocket(const SocketAttribute& sockAttr) :
    Socket(sockAttr) {
}

Socket::UniquePtr SecureSocket::accept() {
    Socket::UniquePtr sock = std::make_unique<SecureSocket>(sockAttr_.family, 
        sockAttr_.type, sockAttr_.protocol);
    int newsock = ::accept(sockFd_, nullptr, nullptr);
    if(-1 == newsock) {
        NEMO_LOG_ERROR(systemLogger) << "accept(" << sockFd_ << ") errno="
            << errno << " errstr=" << strerror(errno);
            sock.reset();
        return nullptr;
    }
    SecureSocket* sSock = down_cast<SecureSocket*>(sock.get());
    sSock->sslCtx_ = sslCtx_;
    if(!sSock->init(newsock)) {
        return nullptr;
    }
    return sock;
}

bool SecureSocket::bind(const Address* addr) {
    return Socket::bind(addr);
}

bool SecureSocket::connect(const Address* address) {
    bool result = Socket::connect(address);
    if(result) {
        sslCtx_.reset(::SSL_CTX_new(SSLv23_client_method()), SslCtxDeleter{});
        ssl_.reset(::SSL_new(sslCtx_.get()));
        ::SSL_set_fd(ssl_.get(), sockFd_);
        result = (::SSL_connect(ssl_.get()) == 1);
    }
    return result;
}

bool SecureSocket::listen(int backlog) {
    return Socket::listen(backlog);
}

bool SecureSocket::close() {
    return Socket::close();
}

int SecureSocket::send(const void* buffer, size_t length, int flags) {
    if(ssl_) {
        return ::SSL_write(ssl_.get(), buffer, length);
    }
    return -1;
}

int SecureSocket::send(const iovec* buffers, size_t length, int flags) {
    if(!ssl_) {
        return -1;
    }
    int total = 0;
    for(size_t i = 0; i < length; ++i) {
        int tmp = ::SSL_write(ssl_.get(), buffers[i].iov_base, buffers[i].iov_len);
        if(tmp <= 0) {
            return tmp;
        }
        total += tmp;
        if(tmp != (int)buffers[i].iov_len) {
            break;
        }
    }
    return total;
}

int SecureSocket::sendTo(const void* buffer, 
    size_t length, const Address* to, int flags) {
    NEMO_LOG_WARN(systemLogger) << "Not implements the method";
    return -1;
}

int SecureSocket::sendTo(const iovec* buffers, 
    size_t length, const Address* to, int flags) {
    NEMO_LOG_WARN(systemLogger) << "Not implements the method";
    return -1;
}

int SecureSocket::recv(void* buffer, size_t length, int flags) {
    if(ssl_) {
        return ::SSL_read(ssl_.get(), buffer, length);
    }
    return -1;
}

int SecureSocket::recv(iovec* buffers, size_t length, int flags) {
    if(!ssl_) {
        return -1;
    }
    int total = 0;
    for(size_t i = 0; i < length; ++i) {
        int tmp = ::SSL_read(ssl_.get(), buffers[i].iov_base, buffers[i].iov_len);
        if(tmp <= 0) {
            return tmp;
        }
        total += tmp;
        if(tmp != (int)buffers[i].iov_len) {
            break;
        }
    }
    return total;
}

int SecureSocket::recvFrom(void* buffer, size_t length, Address* from, int flags) {
    NEMO_LOG_WARN(systemLogger) << "Not implements the method";
    return -1;
}

int SecureSocket::recvFrom(iovec* buffers, size_t length, Address* from, int flags) {
    NEMO_LOG_WARN(systemLogger) << "Not implements the method";
    return -1;
}

bool SecureSocket::loadCertificates(StringArg certFile, StringArg keyFile) {
    sslCtx_.reset(::SSL_CTX_new(SSLv23_server_method()), SslCtxDeleter{});
    if(::SSL_CTX_use_certificate_chain_file(sslCtx_.get(), certFile.data()) != 1) {
        NEMO_LOG_ERROR(systemLogger) << "SSL_CTX_use_certificate_chain_file("
            << certFile << ") error";
        return false;
    }
    if(::SSL_CTX_use_PrivateKey_file(sslCtx_.get(), keyFile.data(), SSL_FILETYPE_PEM) != 1) {
        NEMO_LOG_ERROR(systemLogger) << "SSL_CTX_use_PrivateKey_file("
            << keyFile << ") error";
        return false;
    }
    if(::SSL_CTX_check_private_key(sslCtx_.get()) != 1) {
        NEMO_LOG_ERROR(systemLogger) << "SSL_CTX_check_private_key cert_file="
            << certFile << " key_file=" << keyFile;
        return false;
    }
    return true;
}

std::ostream& SecureSocket::dump(std::ostream& os) const {
    os << "[SecureSocket sock=" << sockFd_
       << " is_connected=" << isConnect_
       << " family=" << sockAttr_.family
       << " type=" << sockAttr_.type
       << " protocol=" << sockAttr_.protocol;
    if(localAddress_) {
        os << " local_address=" << localAddress_->toString();
    }
    if(remoteAddress_) {
        os << " remote_address=" << remoteAddress_->toString();
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) {
    return sock.dump(os);
}

} // namespace net
} // namespace nemo