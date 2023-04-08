#include "net/server.h"

namespace nemo {
namespace net {

Server::Server(const coroutine::Scheduler::SharedPtr& ioScheduler) :
    ioScheduler_(ioScheduler),
    config_(std::make_unique<ServerConfig>()),
    stop_(true) {
    if (nullptr == ioScheduler_) {
        ioScheduler_ = std::make_shared<coroutine::Scheduler>("ServerIo");
    }
}

Server::~Server() {
}

String Server::toString(StringArg prefix) const {
    std::stringstream ss;
    ss << prefix << "[type=" << config_->type
       << " name=" << config_->name << " ssl=" << config_->ssl
       << "\n";
    for(auto& sock : sockets_) {
        ss <<  (prefix.empty() ? "    " : prefix)
           << *sock << std::endl;
    }
    return ss.str();
}

} // namespace net
} // namespace nemo