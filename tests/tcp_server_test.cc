#include "net/tcp_server.h"

#include "log/log.h"
#include "common/macro.h"

static nemo::net::TcpServer::UniquePtr tcpServer = 
    std::make_unique<nemo::net::TcpServer>();
static nemo::net::IpAddress::UniquePtr address;
static nemo::coroutine::Scheduler scheduler;
static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void TcpClient();

int main(int agrc, char** argv) {
    address = nemo::net::Address::LookupAnyIPAddress("0.0.0.0:8080",
        nemo::net::SocketAttribute(AF_INET, SOCK_STREAM, 0));
    NEMO_ASSERT(address);
    address->setPort(8080);
    NEMO_LOG_DEBUG(rootLogger) << "address=" << *address << std::flush;

    NEMO_ASSERT(tcpServer->bind(address.get()));
    tcpServer->start();
    scheduler.addTask(TcpClient);
    scheduler.start();

    return 0;
}

void TcpClient() {
    nemo::net::Socket::UniquePtr sock = nemo::net::Socket::CreateTcpSocket();
    NEMO_ASSERT(sock->connect(address.get()));
    NEMO_LOG_DEBUG(rootLogger) << "client socket=" << *sock;
}