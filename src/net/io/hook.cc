#include "net/io/hook.h"

#include <dlfcn.h>
#include <sys/epoll.h>
#include <sys/poll.h>

#include <memory>

#include "coroutine/processor.h"
#include "util/file_descriptor.h"
#include "net/io/reactor.h"
#include "common/config.h"
#include "log/log.h"


namespace nemo {
namespace net {
namespace io {

static nemo::Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

static nemo::ConfigVar<int>* tcpConnectTimeout =
nemo::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

static thread_local bool hookEnable{false};

bool IsHookEnable() {
    return hookEnable;
}

void SetHookEnable(bool flag) {
    hookEnable = flag;
}

#define HOOK_FUN(XX) \
    XX(sleep)  \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(socketpair) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt) \
    XX(pipe) \
    XX(pipe2) \
    XX(dup) \
    XX(dup2) \
    XX(dup3)

static void HookInit() {
    static bool isInited = false;
    if (isInited) { 
        return;
    }
    isInited = true;
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

static uint64_t connectTimeout = -1;

struct HookIniter {
    HookIniter() {
        HookInit();
        connectTimeout = tcpConnectTimeout->getValue();
        tcpConnectTimeout->addListener([](const int& old_value, const int& new_value){
                NEMO_LOG_INFO(systemLogger) << "tcp connect timeout changed from "
                                         << old_value << " to " << new_value;
                connectTimeout = new_value;
        });
        
    }
};

static HookIniter hoolIniter;

static int Poll(struct pollfd *fds, nfds_t nfds, int timeout, bool nonblocking) {
    if (nonblocking) {
        // 执行一次非阻塞的poll, 检测异常或无效fd.
        int res = ::poll(fds, nfds, 0);
        if (res != 0) {
            return res;
        }
    }

    std::shared_ptr<short int[]> revents = std::make_shared<short int[]>(nfds); //C++20
    
    coroutine::Processor::SuspendEntry entry;
    if (timeout > 0) {
        entry = coroutine::Processor::Suspend(std::chrono::milliseconds(timeout));
    } else {
        entry = coroutine::Processor::Suspend();
    }

    // add file descriptor into epoll or poll.
    bool added = false;
    for (nfds_t i = 0; i < nfds; ++i) {
        pollfd& pfd = fds[i];
        pfd.revents = 0;     // clear revents
        if (pfd.fd < 0) {
            continue;
        }

        if (!net::io::Reactor::Select(pfd.fd)->add(pfd.fd, pfd.events, net::io::Reactor::Entry(i, revents, entry))) {
            // bad file descriptor
            revents[i] = POLLNVAL;
            continue;
        }

        added = true;
    }

    if (!added) {
        // 全部fd都无法加入epoll
        for (nfds_t i = 0; i < nfds; ++i) {
            fds[i].revents = revents[i];
        }
        errno = 0;
        coroutine::Processor::WakeUp(entry);
        coroutine::Processor::Yield();
        return nfds;
    }

    coroutine::Processor::Yield();

    // 唤醒，事件到达
    int triggers = 0;
    for (nfds_t i = 0; i < nfds; ++i) {
        fds[i].revents = revents[i];
        NEMO_LOG_DEBUG(systemLogger) << "fd=" << fds[i].fd 
            << " revent=" << fds[i].events;
        if (fds[i].revents) {
            ++triggers;
        }
    }
    //errno = 0;
    return triggers;
}

class NonBlockingGuard {
public:
    NonBlockingGuard(file_util::FdContext* fdCtx) :
        fdCtx_(fdCtx),
        nonblocking_(fdCtx->isNonBlocking()) {
        if (!nonblocking_) {
            fdCtx_->setNonBlocking(true);
        }
    }
    ~NonBlockingGuard() {
        if (!nonblocking_) {
            fdCtx_->setNonBlocking(false);
        }
    }
private:
    file_util::FdContext* fdCtx_;
    bool nonblocking_;
};

template<typename Fn, typename... Args>
static ssize_t DoIo(int fd, const Fn& f, const char* hookedFnName,
    short int event, int timeout, ssize_t buflen, Args&&... args) {
    using namespace nemo::file_util;

    coroutine::Task* task = coroutine::Processor::GetCurrentRunningTask();
    if (!task) {
        NEMO_LOG_WARN(systemLogger) << "call hook function, name=" 
                << hookedFnName << " but not in coroutine";
        return f(fd, std::forward<Args>(args)...);
    } else {
        NEMO_LOG_DEBUG(systemLogger) << "hook function, name=" 
                << hookedFnName;
    }

    if (!nemo::net::io::IsHookEnable()) {
        return f(fd, std::forward<Args>(args)...);
    }
    
    FdContext* fdCtx = FdManager::GetInstance().get(fd);
    if (!fdCtx || fdCtx->isNonBlocking() || fdCtx->getType() == FdContext::FdType::Plain) {
        return f(fd, std::forward<Args>(args)...);
    }

    int socketTimeout = fdCtx->getSocketTimeoutMicroSeconds(timeout);
    int pollTimeout = (socketTimeout == 0) ? -1 : (socketTimeout < 1000 ? 1 : socketTimeout / 1000);

    struct pollfd fds;
    fds.fd = fd;
    fds.events = event;
    fds.revents = 0;

    do {
        int triggers = nemo::net::io::Poll(&fds, 1, pollTimeout, true);
        if (-1 == triggers) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        } else if (0 == triggers) {  // poll等待超时
            errno = EAGAIN;
            return -1;
        }
        break;
    } while (true);

    // 等待事件到达后再次尝试
    ssize_t result = -1;
    do {
        result = f(fd, std::forward<Args>(args)...);
        if (-1 == result && EINTR == errno) {
            continue;
        }
        break;
    } while (true);
    NEMO_LOG_DEBUG(systemLogger) << "hook_fun_name=" << hookedFnName
        << " fd=" << fd
        << " result=" << result;

    return result;
}

} // namespace io
} // namespace net
} // namespace nemo

extern "C" {

#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds) {
    if (nemo::coroutine::Processor::GetCurrentRunningTask() && nemo::net::io::IsHookEnable()) {
        nemo::coroutine::Processor::Suspend(std::chrono::seconds(seconds));
        nemo::coroutine::Processor::Yield();
        return 0;
    }

    return sleep_f(seconds);
}

int usleep(useconds_t usecond) {
    if (nemo::coroutine::Processor::GetCurrentRunningTask() && nemo::net::io::IsHookEnable()) {
        nemo::coroutine::Processor::Suspend(std::chrono::milliseconds(usecond));
        nemo::coroutine::Processor::Yield();
        return 0;
    }

    return usleep_f(usecond);
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    constexpr static time_t kNanoSecondsPerSeconds = 1e9;
    if (nemo::coroutine::Processor::GetCurrentRunningTask() && nemo::net::io::IsHookEnable()) {
        nemo::coroutine::Processor::Suspend(std::chrono::nanoseconds(req->tv_sec * kNanoSecondsPerSeconds + req->tv_nsec));
        nemo::coroutine::Processor::Yield();
        return 0;
    }

    return nanosleep_f(req, rem);
}

int socket(int domain, int type, int protocol) {
    int sockfd = socket_f(domain, type, protocol);
    if (nemo::net::io::IsHookEnable()) {
        if (sockfd > 0) {
            nemo::file_util::FdManager::GetInstance().add(
                std::make_unique<nemo::file_util::FdContext>(sockfd, 
                    nemo::file_util::FdContext::FdType::Socket, 
                    false,
                    nemo::net::SocketAttribute(domain, type, protocol)));
        }
    }
    return sockfd;
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    int result = socketpair_f(domain, type, protocol, sv);
    if (nemo::net::io::IsHookEnable()) {
        if (0 == result) {
            nemo::file_util::FdManager::GetInstance().add(
                std::make_unique<nemo::file_util::FdContext>(sv[0], 
                    nemo::file_util::FdContext::FdType::Socket, 
                    false,
                    nemo::net::SocketAttribute(domain, type, protocol)));
            nemo::file_util::FdManager::GetInstance().add(
                std::make_unique<nemo::file_util::FdContext>(sv[1], 
                    nemo::file_util::FdContext::FdType::Socket, 
                    false,
                    nemo::net::SocketAttribute(domain, type, protocol)));
        }
    }

    return result;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return connect_f(sockfd, addr, addrlen);
    }

    nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(sockfd);
    if (!fdCtx || !fdCtx->isTcpSocket() || fdCtx->isNonBlocking()) {
        return connect_f(sockfd, addr, addrlen);
    }
    
    int result{0};
    {
        nemo::net::io::NonBlockingGuard nonBlockingGuard(fdCtx);
        result = connect_f(sockfd, addr, addrlen);
    }

    if (0 == result) {
        return 0;
    } else { //-1 == result
        if (errno != EINPROGRESS) {
            return result;
        }
    }

    //poll
    int connectTimeout = fdCtx->getTcpConnectTimeout();
    int pollTimeout = (connectTimeout == 0) ? -1 : connectTimeout;
    struct pollfd pfd[1];
    pfd[0].fd = sockfd;
    pfd[0].events = POLLOUT;
    int triggers = nemo::net::io::Poll(pfd, 1, pollTimeout, false);
    if (triggers <= 0 || pfd[0].revents != POLLOUT) {
        errno = ETIMEDOUT;
        return -1;
    }

    int sockErr = 0;
    socklen_t len = sizeof(int);
    if (-1 == ::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockErr, &len)) {
        return -1;
    }

    if (!sockErr) {
        return 0;
    }

    errno = sockErr;
    return -1;
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return accept_f(s, addr, addrlen);
    }

    nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(s);
    if (!fdCtx) {
        errno = EBADF;
        return -1;
    }

    int sockfd = nemo::net::io::DoIo(s, accept_f, "accept", POLLIN, SO_RCVTIMEO, 0, addr, addrlen);
    if (sockfd > 0) {
        nemo::file_util::FdManager::GetInstance().add(
                std::make_unique<nemo::file_util::FdContext>(sockfd, 
                    nemo::file_util::FdContext::FdType::Socket, 
                    false,
                    fdCtx->getSocketAttribute()));
    }

    return sockfd;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return read_f(fd, buf, count);
    }

    return nemo::net::io::DoIo(fd, read_f, "read", POLLIN, SO_RCVTIMEO, count, 
        buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return readv_f(fd, iov, iovcnt);
    }

    int buflen{0};
    for (int i = 0; i < iovcnt; ++i) {
        buflen += iov[i].iov_len;
    }

    return nemo::net::io::DoIo(fd, readv_f, "readv", POLLIN, SO_RCVTIMEO, buflen, 
        iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return recv_f(sockfd, buf, len, flags);
    }

    return nemo::net::io::DoIo(sockfd, recv_f, "recv", POLLIN, SO_RCVTIMEO, len, 
        buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, 
    struct sockaddr *src_addr, socklen_t *addrlen) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return recvfrom_f(sockfd, buf, len, flags, src_addr, addrlen);
    }
    return nemo::net::io::DoIo(sockfd, recvfrom_f, "recvfrom", POLLIN, 
            SO_RCVTIMEO, len, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return recvmsg_f(sockfd, msg, flags);
    }

    size_t buflen = 0;
    for (size_t i = 0; i < msg->msg_iovlen; ++i) {
        buflen += msg->msg_iov[i].iov_len;
    }
    return nemo::net::io::DoIo(sockfd, recvmsg_f, "recvmsg", POLLIN, SO_RCVTIMEO, 
        buflen, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return write_f(fd, buf, count);
    }

    return nemo::net::io::DoIo(fd, write_f, "write", POLLOUT, SO_SNDTIMEO, 
        count, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return writev_f(fd, iov, iovcnt);
    }

    size_t buflen{0};
    for (int i = 0; i < iovcnt; ++i) {
        buflen += iov[i].iov_len;
    }
    return nemo::net::io::DoIo(fd, writev_f, "writev", POLLOUT, SO_SNDTIMEO, 
        buflen, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return send_f(s, msg, len, flags);
    }

    return nemo::net::io::DoIo(s, send_f, "send", POLLOUT, SO_SNDTIMEO, 
        len, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return sendto_f(s, msg, len, flags, to, tolen);
    }

    return nemo::net::io::DoIo(s, sendto_f, "sendto", POLLOUT, SO_SNDTIMEO, len, 
        msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    if (!nemo::net::io::IsHookEnable() || !nemo::coroutine::Processor::GetCurrentRunningTask()) {
        return sendmsg_f(s, msg, flags);
    }

    size_t buflen = 0;
    for (size_t i = 0; i < msg->msg_iovlen; ++i) {
        buflen += msg->msg_iov[i].iov_len;
    }
    return nemo::net::io::DoIo(s, sendmsg_f, "sendmsg", POLLOUT, SO_SNDTIMEO, buflen, 
        msg, flags);
}

int close(int fd) {
    if (!nemo::net::io::IsHookEnable()) {
        return close_f(fd);
    }

    nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(fd);
    if (fdCtx) {
        fdCtx->onClose();
        nemo::file_util::FdManager::GetInstance().erase(fdCtx);
    }

    
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL: {
                int flags = va_arg(va, int);
                va_end(va);

                nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(fd);
                if (fdCtx) {
                    bool isNonBlocking = !!(flags & O_NONBLOCK);
                    fdCtx->onSetNonBlocking(isNonBlocking);
                }
                return fcntl_f(fd, cmd, flags);
            }
            break;
        case F_GETFL: {
                va_end(va);		//GET是void类型，只传了两个参数
                int arg = fcntl_f(fd, cmd);
                nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(fd);
                if(!fdCtx || !fdCtx->isSocket()) {
                    return arg;
                }
                if(fdCtx->isNonBlocking()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        // int
        case F_DUPFD:
            [[fallthrough]];
        case F_DUPFD_CLOEXEC: {
                int arg = va_arg(va, int);
                va_end(va);
                int newfd = fcntl_f(fd, cmd, arg);
                if (newfd < 0) {
                    return newfd;
                }
                nemo::file_util::FdContext* oldFdCtx = nemo::file_util::FdManager::GetInstance().get(fd);
                if (oldFdCtx) {
                    nemo::file_util::FdManager::GetInstance().add(oldFdCtx->clone(newfd));
                }
                return newfd;
            }
            break;
        case F_SETFD:
            [[fallthrough]];
        case F_SETOWN:
            [[fallthrough]];
        case F_SETSIG:
            [[fallthrough]];
        case F_SETLEASE:
            [[fallthrough]];
        case F_NOTIFY:
            [[fallthrough]];
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);	//这些情况都是int类型的参数
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        // void
        case F_GETFD:
            [[fallthrough]];
        case F_GETOWN:
            [[fallthrough]];
        case F_GETSIG:
            [[fallthrough]];
        case F_GETLEASE:
            [[fallthrough]];
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        // struct flock*
        case F_SETLK:
            [[fallthrough]];
        case F_SETLKW:
            [[fallthrough]];
        case F_GETLK: {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        // struct f_owner_exlock*
        case F_GETOWN_EX:
            [[fallthrough]];
        case F_SETOWN_EX: {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:		
            va_end(va);
            return fcntl_f(fd, cmd);
    }

    va_end(va);
    return fcntl_f(fd, cmd);
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if (FIONBIO == request) {
        nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(d);
        if (fdCtx) {
            bool isNonBlocking = !!*(int*)arg;
            fdCtx->onSetNonBlocking(isNonBlocking);
        }
    }

    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    int res = setsockopt_f(sockfd, level, optname, optval, optlen);

    if (0 == res && level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(sockfd);
            if (fdCtx) {
                const timeval & tv = *(const timeval*)optval;
                int microseconds = tv.tv_sec * 1000000 + tv.tv_usec;
                fdCtx->onSetSocketTimeout(optname, microseconds);
            }
        }
    }

    return res;
}

int pipe(int pipefd[2]) {
    int result = pipe_f(pipefd);

    if (nemo::net::io::IsHookEnable() && 0 == result) {
        nemo::file_util::FdManager::GetInstance().add(std::make_unique<nemo::file_util::FdContext>(
            pipefd[0],
            nemo::file_util::FdContext::FdType::Pipe,
            false,
            nemo::net::SocketAttribute{}
        ));
        nemo::file_util::FdManager::GetInstance().add(std::make_unique<nemo::file_util::FdContext>(
            pipefd[1],
            nemo::file_util::FdContext::FdType::Pipe,
            false,
            nemo::net::SocketAttribute{}
        ));
    }

    return result;
}

int pipe2(int pipefd[2], int flags) {
    int result = pipe2_f(pipefd, flags);

    if (nemo::net::io::IsHookEnable() && 0 == result) {
        nemo::file_util::FdManager::GetInstance().add(std::make_unique<nemo::file_util::FdContext>(
            pipefd[0],
            nemo::file_util::FdContext::FdType::Pipe,
            false,
            nemo::net::SocketAttribute{}
        ));
        nemo::file_util::FdManager::GetInstance().add(std::make_unique<nemo::file_util::FdContext>(
            pipefd[1],
            nemo::file_util::FdContext::FdType::Pipe,
            false,
            nemo::net::SocketAttribute{}
        ));
    }

    return result;
}

int dup(int oldfd) {
    int newfd = dup_f(oldfd);
    if (newfd < 0) {
        return newfd;
    }

    nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(oldfd);
    if (fdCtx) {
        nemo::file_util::FdManager::GetInstance().add(fdCtx->clone(newfd));
    }
    return newfd;
}

int dup2(int oldfd, int newfd) {
    if (newfd < 0 || oldfd < 0 || oldfd == newfd) {
        return dup2_f(oldfd, newfd);
    }

    int ret = dup2_f(oldfd, newfd);
    if (ret < 0) {
        return ret;
    }

    nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(oldfd);
    if (fdCtx) {
        nemo::file_util::FdManager::GetInstance().add(fdCtx->clone(newfd));
    }
    return ret;
}

int dup3(int oldfd, int newfd, int flags) {
    if (newfd < 0 || oldfd < 0 || oldfd == newfd) {
        return dup3_f(oldfd, newfd, flags);
    }

    int ret = dup3_f(oldfd, newfd, flags);
    if (ret < 0) {
        return ret;
    }

    nemo::file_util::FdContext* fdCtx = nemo::file_util::FdManager::GetInstance().get(oldfd);
    if (fdCtx) {
        nemo::file_util::FdManager::GetInstance().add(fdCtx->clone(newfd));
    }
    return ret;
}

} // extern "C"