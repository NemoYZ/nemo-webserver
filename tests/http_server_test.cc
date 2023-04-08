#include "net/http/http_server.h"

#include "coroutine/coroutine.h"
#include "log/log.h"
#include "common/config.h"

using namespace nemo;

static net::http::HttpServer::UniquePtr server = std::make_unique<net::http::HttpServer>();
static Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void Test();

int main(int argc, char** argv) {
    //nemo::Config::LoadFromDir("/programs/nemo/config");
    coroutine_async Test;
    coroutine_start;

    return 0;
}

void Test() {
    net::IpAddress::UniquePtr addr = net::Address::LookupAnyIPAddress("0.0.0.0:8080");
    if (!addr) {
        NEMO_LOG_ERROR(rootLogger) << "lookup address error";
        return;
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "address=" << *addr;
    }
    if(!server->bind(addr.get())) {
        NEMO_LOG_ERROR(rootLogger) << "bind address error";
        return;
    }
    net::http::ServletDispatcher* servletDispatch = server->getServletDispatcher();
    servletDispatch->addServlet("/Nemo/xx", [](net::http::HttpRequest* request,
                net::http::HttpResponse* response,
                net::http::HttpSession* session) {
            response->setBody(request->toString());
            return 0;
    });

    servletDispatch->addGlobServlet("/Nemo/*", [](net::http::HttpRequest* request,
                net::http::HttpResponse* response,
                net::http::HttpSession* session) {
            response->setBody("Glob:\r\n" + request->toString());
            return 0;
    });
    server->start();
}