#include <vector>

#include "net/socket.h"
#include "log/log.h"
#include "coroutine/scheduler.h"
#include "common/types.h"
#include "common/macro.h"

static nemo::coroutine::Scheduler scheduler;
static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");
static nemo::net::IpAddress::UniquePtr address;

void Test();
void EchoServer();
void EchoClient();

int main(int argc, char** argv) {
    address = nemo::net::Address::LookupAnyIPAddress("localhost", 
        nemo::net::SocketAttribute(AF_INET, SOCK_STREAM, 0));
    NEMO_ASSERT(address);
    address->setPort(8080);

    scheduler.addTask(EchoServer);
    scheduler.addTask(EchoClient);

    scheduler.start();

    // scheduler.addTask(Test);
    // scheduler.localStart();

    return 0;
}

void Test() {
    std::vector<nemo::net::Address::UniquePtr> addrs;
    nemo::net::Address::Lookup("www.baidu.com", std::back_insert_iterator<decltype(addrs)>(addrs),
        nemo::net::SocketAttribute(AF_INET, SOCK_STREAM, 0));
    nemo::net::IpAddress::UniquePtr addr;

    for(auto& address : addrs) {
        NEMO_LOG_INFO(rootLogger) << address->toString();
        if (address->getFamily() == AF_INET) {
            addr.reset(dynamic_cast<nemo::net::IpAddress*>(address.get()));
            address.release();
        }
    }

    // nemo::net::IpAddress::UniquePtr addr;
    // nemo::net::Address::LookupAnyIPAddress("www.baidu.com", addr, 
    //     nemo::net::SocketAttribute(AF_INET, SOCK_STREAM, 0));
    if(addr) {
        NEMO_LOG_INFO(rootLogger) << "get address: " << addr->toString();
    } else {
        NEMO_LOG_INFO(rootLogger) << "get address fail";
        return;
    }

    nemo::net::Socket::UniquePtr sock = nemo::net::Socket::CreateTcp(addr.get());
    if (!sock) {
        NEMO_LOG_ERROR(rootLogger) << "socket create failed";
        return;
    }
    addr->setPort(80);
    NEMO_LOG_INFO(rootLogger) << "addr=" << addr->toString();
    if(!sock->connect(addr.get())) {
        NEMO_LOG_INFO(rootLogger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        NEMO_LOG_INFO(rootLogger) << "connect " << sock->getRemoteAddress()->toString() << " connected";
    }

    const char sendBuff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(sendBuff, sizeof(sendBuff));
    if(rt <= 0) {
        NEMO_LOG_INFO(rootLogger) << "send fail rt=" << rt;
        return;
    }

    nemo::String recvBuff;
    recvBuff.resize(4096);
    rt = sock->recv(recvBuff.data(), recvBuff.size());

    if(rt <= 0) {
        NEMO_LOG_INFO(rootLogger) << "recv fail rt=" << rt;
        return;
    }

    recvBuff.resize(rt);
    NEMO_LOG_INFO(rootLogger) << recvBuff;
}

void EchoServer() {
    nemo::net::Socket::UniquePtr acceptSock = nemo::net::Socket::CreateTcpSocket();
    NEMO_ASSERT(acceptSock);
    int result = acceptSock->bind(address.get());
    if (-1 == result) {
        NEMO_LOG_ERROR(rootLogger) << "bind error, please change the port value";
        exit(1);
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "echo_server bind succeed";
    }
    NEMO_ASSERT(acceptSock->listen());
    

    while (true) {
        nemo::net::Socket::UniquePtr clientSock = acceptSock->accept();
        NEMO_ASSERT(clientSock);
        NEMO_LOG_DEBUG(rootLogger) << "in server, client socket=" << *clientSock;
        char buf[64]{0};
        
        do {
            int n = clientSock->recv(buf, sizeof buf, 0);
            if (-1 == n) {
                if (EAGAIN == errno || EINTR == errno) {
                    continue;
                } else {
                    NEMO_LOG_ERROR(rootLogger) << "read error, errno="
                        << errno << " strerr=" << strerror(errno);
                }
            } else if (n == 0) {
                NEMO_LOG_ERROR(rootLogger) << "read eof";
            } else {
                NEMO_LOG_DEBUG(rootLogger) << "server recevied, buf=" << buf;
                ssize_t wn = clientSock->send(buf, n, 0);
                static_cast<void>(wn);
            }
            break;
        } while (true);
    }
}

void EchoClient() {
    nemo::net::Socket::UniquePtr sock = nemo::net::Socket::CreateTcp(address.get());
    // 阻塞的connect已被HOOK，等待期间切换执行其他协程。
    if (!sock->connect(address.get())) {
        NEMO_LOG_ERROR(rootLogger) << "connect error, errno=" << errno
            << " strerr=" << strerror(errno);
        exit(1);
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "client connect succeed";
    }

    NEMO_LOG_DEBUG(rootLogger) << "in client, server socket=" << *sock;
    
    char msg[] = "My name is Van";

    // 阻塞的write已被HOOK，等待期间切换执行其他协程。
    NEMO_LOG_DEBUG(rootLogger) << "client write";
    //ssize_t wn = ::write(sockfd, msg, sizeof msg);
    ssize_t wn = sock->send(msg, sizeof msg);
    if (-1 == wn) {
        NEMO_LOG_ERROR(rootLogger) << "send error, errno=" << errno
                    << " strerr=" << strerror(errno);
        return;
    }

    char buf[64]{0};
    do {
        // 阻塞的read已被HOOK，等待期间切换执行其他协程。
        //int n = ::read(sockfd, buf, sizeof(buf));
        int n = sock->recv(buf, sizeof buf);
        if (n == -1) {
            if (EAGAIN == errno || EINTR == errno) {
                continue;
            } else {
                NEMO_LOG_ERROR(rootLogger) << "read error, errno=" << errno
                    << " strerr=" << strerror(errno);
            }
        } else if (n == 0) {
            NEMO_LOG_ERROR(rootLogger) << "read eof";
        } else {
            NEMO_LOG_DEBUG(rootLogger) << "client read finished, buf=" << buf;
        }
        break;
    } while (true);
}
