#include "net/http/http_server.h"

#include "log/log.h"
#include "common/macro.h"

namespace nemo {
namespace net {
namespace http {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive,
        const coroutine::Scheduler::SharedPtr& ioScheduler,
        const coroutine::Scheduler::SharedPtr& acceptScheduler,
        const coroutine::Scheduler::SharedPtr&  handleScheduler) :
    TcpServer(ioScheduler, acceptScheduler, handleScheduler),
    dispatcher_(std::make_unique<ServletDispatcher>()),
    keepalive_(keepalive) {
    config_->type = "http";
}

void HttpServer::handleClient(Socket::SharedPtr client) {
    NEMO_LOG_DEBUG(systemLogger) << "handleClient " << *client;
    HttpSession::UniquePtr session = std::make_unique<HttpSession>(client.get());
    do {
        HttpRequest::UniquePtr request = session->recvRequest();
        if(!request) {
            NEMO_LOG_WARN(systemLogger) << "recv http request fail, errno="
                << errno << " errstr=" << strerror(errno)
                << " cliet:" << *client << " keep_alive=" << keepalive_;
            break;
        }

        HttpResponse::UniquePtr response = std::make_unique<HttpResponse>(request->getVersion(),
                        request->isClose() || !keepalive_);
        response->setHeader("Server", getName());
        dispatcher_->handle(request.get(), response.get(), session.get());
        session->sendResponse(response.get());

        if(!keepalive_ || request->isClose() || !client->isConnect()) {
            break;
        }
    } while(true);
}

} // namespace http
} // namespace net
} // namespace nemo