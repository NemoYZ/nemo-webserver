#include "net/tcp_server.h"

#include "log/log.h"
#include "common/config.h"
#include "common/macro.h"

namespace nemo {
namespace net {

static ConfigVar<uint64_t>* tcpServerReadTimeout =
   Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2),
            "tcp server read timeout");
static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

void TcpServer::handleClient(Socket::SharedPtr client) {
    //TODO 
    // 解决使用unique_ptr传参数导致进入函数之前client先释放问题
    NEMO_LOG_INFO(systemLogger) << "handleClient: " << *client
        << " client.get()=" << client.get();
}

void TcpServer::startAccept(Socket* sock) {
    while(!stop_) {
        Socket::UniquePtr clientSock = sock->accept();
        if(clientSock) {
            clientSock->setRecvTimeout(recvTimeoutMillionSeconds_);
            Socket::SharedPtr clientShared = std::move(clientSock);
            NEMO_ASSERT(clientShared->getSocketFd() != -1);
            handleScheduler_->addTask([clientShared, this](){
                this->handleClient(clientShared);
            });
        } else {
            NEMO_LOG_ERROR(systemLogger) << "accept errno=" << errno
                << " errstr=" << strerror(errno);
        }
    }
}

bool TcpServer::bindAddress(const Address* address) {
    Socket::UniquePtr sock = Socket::CreateTcp(address);
    if(!sock->bind(address)) {
        NEMO_LOG_ERROR(systemLogger) << "bind fail errno="
            << errno << " errstr=" << strerror(errno)
            << " address=[" << address->toString() << "]";
        return false;
    }
    if(!sock->listen()) {
        NEMO_LOG_ERROR(systemLogger) << "listen fail errno="
            << errno << " errstr=" << strerror(errno)
            << " address=[" << address->toString() << "]";
        return false;
    }
    sockets_.emplace_back(std::move(sock));
    return true;
}

TcpServer::TcpServer(const coroutine::Scheduler::SharedPtr& ioScheduler,
        const coroutine::Scheduler::SharedPtr& acceptScheduler,
        const coroutine::Scheduler::SharedPtr& handleScheduler) : 
    Server(ioScheduler),
    acceptScheduler_(acceptScheduler),
    handleScheduler_(handleScheduler),
    recvTimeoutMillionSeconds_(tcpServerReadTimeout->getValue()) {
    if (nullptr == acceptScheduler_) {
        acceptScheduler_ = ioScheduler_;
    }
    if (nullptr == handleScheduler_) {
        handleScheduler_= std::make_shared<coroutine::Scheduler>("TcpServerHandle");
    }
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::bind(const Address* address, bool ssl) { 
    bool bindSuccess = bindAddress(address);
    if (bindSuccess) {
        config_->ssl = ssl;
    }

    return bindSuccess;
}

bool TcpServer::bind(const std::vector<Address::UniquePtr>& addresses,
        std::vector<Address*>& fails, bool ssl) {
    for(auto& address : addresses) {
        bool bindSuccess = bindAddress(address.get());
        if (!bindSuccess) {
            fails.emplace_back(address.get());
        }
    }

    if (addresses.size() != fails.size()) {
        config_->ssl = ssl;
    }

    return fails.empty();
}

bool TcpServer::start() {
    if(!stop_) {
        return true;
    }
    stop_ = false;
    
    for(auto& sock : sockets_) {
        acceptScheduler_->addTask([this, &sock](){
            startAccept(sock.get());
        });
    }
    acceptScheduler_->threadStart();
    ioScheduler_->threadStart();
    handleScheduler_->threadStart();
    
    return true;
}

void TcpServer::stop() {
    stop_ = true;
    acceptScheduler_->stop();
    ioScheduler_->stop();
    handleScheduler_->stop();

    sockets_.clear();
}

bool TcpServer::loadCertificates(StringArg certFile, StringArg keyFile) {
    for (size_t i = 0; i < sockets_.size(); ++i) {
        SecureSocket* sSocket = dynamic_cast<SecureSocket*>(sockets_[i].get());
        if (sSocket) {
            if(!sSocket->loadCertificates(certFile, keyFile)) {
                return false;
            }
        }
    }

    return true;
}

String TcpServer::toString(StringArg prefix) const {
    std::stringstream ss;
    ss << prefix << "[type=" << config_->type
       << " name=" << config_->name << " ssl=" << config_->ssl
       << " recv_timeout=" << recvTimeoutMillionSeconds_ << "]" 
       << "\n";
    for(auto& sock : sockets_) {
        ss <<  (prefix.empty() ? "    " : prefix)
           << *sock << std::endl;
    }
    return ss.str();
}

} // namespace net
} // namespace nemo