#include "net/io/epoll_reactor.h"

#include <poll.h>
#include <sys/epoll.h>

#include "util/file_descriptor.h"
#include "log/log.h"
#include "common/macro.h"

namespace nemo {
namespace net {
namespace io {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

uint32_t PollEvent2EpollEvent(short int pollEvent) {
    uint32_t epollEvent = 0;
    if (pollEvent & POLLIN) {
        epollEvent |= EPOLLIN;
    }
    if (pollEvent & POLLOUT) {
        epollEvent |= EPOLLOUT;
    }
    if (pollEvent & POLLERR) {
        epollEvent |= EPOLLERR;
    }
    if (pollEvent & POLLHUP) {
        epollEvent |= EPOLLHUP;
    }

    return epollEvent;
}

short int EpollEvent2PollEvent(uint32_t epollEvent) {
    short int pollEvent = 0;
    if (epollEvent & EPOLLIN) {
        pollEvent |= POLLIN;
    }
    if (epollEvent & EPOLLOUT) {
        pollEvent |= POLLOUT;
    }
    if (epollEvent & EPOLLERR) {
        pollEvent |= POLLERR;
    }
    if (epollEvent & EPOLLHUP) {
        pollEvent |= POLLHUP;
    }

    return pollEvent;
}

EpollReactor::EpollReactor() :
    epfd_(::epoll_create(MAX_EVENTS)) {
    NEMO_ASSERT(epfd_ > 0);
}

bool EpollReactor::addEvent(int fd, short int event, short int promiseEvent) {
    struct epoll_event epollEvent{};
    epollEvent.events = PollEvent2EpollEvent(promiseEvent) | EPOLLET;
    epollEvent.data.fd = fd;
    
    int op = event == promiseEvent ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    int res = InvokeSlowSystemCall(::epoll_ctl, epfd_, op, fd, &epollEvent);
    return res == 0;
}

bool EpollReactor::delEvent(int fd, short int event, short int promiseEvent) {
    struct epoll_event epollEvent;
    epollEvent.events = PollEvent2EpollEvent(promiseEvent) | EPOLLET;
    epollEvent.data.fd = fd;
    int op = promiseEvent == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    int res = InvokeSlowSystemCall(::epoll_ctl, epfd_, op, fd, &epollEvent);
    return res == 0;
}

void EpollReactor::run() {
    struct epoll_event evs[MAX_EVENTS]{0};
    int n = InvokeSlowSystemCall(::epoll_wait, epfd_, evs, MAX_EVENTS, MAX_TIMEOUT);
    if (n < 0) {
        NEMO_LOG_ERROR(systemLogger) << "epoll_wait error, errno=" << errno
            << " errstr=" << strerror(errno);
    }
    
    for (int i = 0; i < n; ++i) {
        struct epoll_event& ev = evs[i];
        int fd = ev.data.fd;
        NEMO_LOG_DEBUG(systemLogger) << "trigger event, fd=" << fd 
            << " event=" << ev.events;
        file_util::FdContext* fdCtx = file_util::FdManager::GetInstance().get(fd);
        if (fdCtx) {
            fdCtx->trigger(this, EpollEvent2PollEvent(ev.events));
        }
    }
}

} // namespace io
} // namespace net
} // namespace nemo