#include "net/io/reactor.h"

#include "util/file_descriptor.h"
#include "common/thread.h"
#include "net/io/epoll_reactor.h"
#include "log/log.h"
#include "common/macro.h"

namespace nemo {
namespace net {
namespace io {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");
std::vector<Reactor::UniquePtr> Reactor::reactors_;

int Reactor::InitReactors(int n) {
    if (!reactors_.empty()) {
        return -1;
    }

    reactors_.reserve(n);
    for (int i = 0; i < n; ++i) {
        reactors_.emplace_back(std::make_unique<EpollReactor>());
        reactors_.back()->start();
    }
    
    return 0;
}

Reactor::Reactor() :
    started_{false} {
}

Reactor::~Reactor() {
    stop();
}

void Reactor::start() {
    if (started_.load(std::memory_order::acquire)) {
        return;
    }
    started_.store(true, std::memory_order::release);
    
    thread_ = std::make_unique<Thread>([this](){
        while (NEMO_LIKELY(this->started_)) {
            this->run();
        }
    }, "Reactor");
    thread_->start();
}

void Reactor::stop() {
    if (!started_.load(std::memory_order::acquire)) {
        return;
    }
    started_.store(false, std::memory_order::release);
    thread_->join();
}

bool Reactor::add(int fd, short int pollEvent, const Entry& entry) {
    file_util::FdContext* fdCtx = file_util::FdManager::GetInstance().get(fd);
    if (!fdCtx) {
        return false;
    }

    return fdCtx->add(this, pollEvent, entry);
}

} // namespace io
} // namespace net
} // namespace nemo