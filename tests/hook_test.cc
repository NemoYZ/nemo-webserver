#include <arpa/inet.h>
#include <netdb.h>

#include "net/io/hook.h"
#include "log/log.h"
#include "util/util.h"
#include "coroutine/processor.h"
#include "coroutine/scheduler.h"
#include "util/file_descriptor.h"
#include "common/macro.h"

using namespace nemo;

static Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");
constexpr static int port = 42225;
static coroutine::Scheduler scheduler1;
static const char* ipaddress = "192.168.66.3";

void TestSleep();
void TestEcho();
void EchoServer();
void Client();

int main(int argc, char** argv) {
    //TestSleep();
    TestEcho();

    return 0;
}

void TestSleep() {
    scheduler1.addTask([](){
        sleep(2);
        NEMO_LOG_DEBUG(rootLogger) << "sleep2";
    });
    scheduler1.addTask([](){
        sleep(3);
        NEMO_LOG_DEBUG(rootLogger) << "sleep3";
    });
    scheduler1.start();
}

void TestEcho() {
    scheduler1.addTask(EchoServer);
    scheduler1.addTask(Client);
    scheduler1.start();
}

void EchoServer() {
    nemo::net::io::SetHookEnable(true);
    NEMO_LOG_DEBUG(rootLogger) << "echo_server";
    int accept_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    NEMO_ASSERT(accept_fd > 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipaddress);
    socklen_t len = sizeof(addr);
    if (-1 == ::bind(accept_fd, (sockaddr*)&addr, len)) {
        NEMO_LOG_ERROR(rootLogger) << "bind error, please change the port value";
        exit(1);
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "echo_server bind succeed";
    }
    if (-1 == ::listen(accept_fd, 5)) {
        NEMO_LOG_ERROR(rootLogger) << "listen error";
        exit(1);
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "echo_server listen succeed";
    }

    while (true) {
        int sockfd = -1;
        do {
        // 阻塞的accept已被HOOK，等待期间切换执行其他协程。
            sockfd = ::accept(accept_fd, (sockaddr*)&addr, &len);
            if (-1 == sockfd) {
                if (EAGAIN == errno || EINTR == errno) {
                    NEMO_LOG_DEBUG(rootLogger) << "error=" << strerror(errno);
                    continue;
                } else {
                    NEMO_LOG_ERROR(rootLogger) << "accept error, errno=" 
                        << errno << " strerr=" << strerror(errno);
                    return;
                }
            } else {
                NEMO_LOG_DEBUG(rootLogger) << "echo_server accept succeed";
            }
            break;
        } while (true);

        char buf[64]{0};
        
        do {
            // 阻塞的read已被HOOK，等待期间切换执行其他协程。
            //int n = ::read(sockfd, buf, sizeof(buf));
            int n = ::recv(sockfd, buf, sizeof buf, 0);
            if (n == -1) {
                if (EAGAIN == errno || EINTR == errno) {
                    continue;
                } else {
                    NEMO_LOG_ERROR(rootLogger) << "read error, errno="
                        << errno << " strerr=" << strerror(errno);
                }
            } else if (n == 0) {
                NEMO_LOG_ERROR(rootLogger) << "read eof";
            } else {
                // echo
                // 阻塞的write已被HOOK，等待期间切换执行其他协程。
                NEMO_LOG_DEBUG(rootLogger) << "server recevied, buf=" << buf;
                //ssize_t wn = ::write(sockfd, buf, n);
                ssize_t wn = ::send(sockfd, buf, n, 0);
                static_cast<void>(wn);
            }
            break;
        } while (true);
        ::close(sockfd);
    }
}

void Client() {
    NEMO_LOG_DEBUG(rootLogger) << "client";
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    NEMO_ASSERT(sockfd > 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ::inet_addr(ipaddress);
    // 阻塞的connect已被HOOK，等待期间切换执行其他协程。
    NEMO_LOG_DEBUG(rootLogger) << "client connet";
    NEMO_ASSERT(connect_f);
    if (-1 == ::connect(sockfd, (sockaddr*)&addr, sizeof(addr))) {
        NEMO_LOG_ERROR(rootLogger) << "connect error, errno=" << errno
            << " strerr=" << strerror(errno);
        exit(1);
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "client connect succeed";
    }
    
    char msg[] = "My name is Van";

    // 阻塞的write已被HOOK，等待期间切换执行其他协程。
    NEMO_LOG_DEBUG(rootLogger) << "client write";
    //ssize_t wn = ::write(sockfd, msg, sizeof msg);
    ssize_t wn = ::send(sockfd, msg, sizeof msg, 0);
    if (-1 == wn) {
        NEMO_LOG_ERROR(rootLogger) << "send error, errno=" << errno
                    << " strerr=" << strerror(errno);
        return;
    }

    char buf[64]{0};
    do {
        // 阻塞的read已被HOOK，等待期间切换执行其他协程。
        //int n = ::read(sockfd, buf, sizeof(buf));
        int n = ::recv(sockfd, buf, sizeof(buf), 0);
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
    ::close(sockfd);
}