#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

#include "coroutine/scheduler.h"
#include "net/io/reactor.h"
#include "log/log.h"
#include "net/io/epoll_reactor.h"
#include "net/io/hook.h"
#include "common/macro.h"
#include "common/thread.h"
#include "util/file_descriptor.h"

static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");
static nemo::coroutine::Scheduler scheduler("scheduler1");
static int fds[2];

void Wait();
void Send();

int main(int argc, char** argv) {
    int result = ::pipe(fds);
    if (result < 0) {
        NEMO_LOG_DEBUG(rootLogger) << "pipe error, errno="
            << errno << " strerr=" << strerror(errno);
    }
    nemo::file_util::FdManager::GetInstance().add(
        std::make_unique<nemo::file_util::FdContext>(fds[0],
            nemo::file_util::FdContext::FdType::Pipe,
            false,
            nemo::net::SocketAttribute{}));
    nemo::file_util::FdManager::GetInstance().add(
        std::make_unique<nemo::file_util::FdContext>(fds[1],
            nemo::file_util::FdContext::FdType::Pipe,
            false,
            nemo::net::SocketAttribute{}));

    scheduler.addTask(Wait);
    scheduler.addTask(Send);
    scheduler.start();

    return 0;
}

void Wait() {
    std::shared_ptr<short int[]> events = std::make_shared<short int[]>(1);
    nemo::coroutine::Processor::SuspendEntry suspendEntry = nemo::coroutine::Processor::Suspend();
    nemo::net::io::Reactor::Select(fds[0])->add(fds[0], POLLIN, nemo::net::io::Reactor::Entry(0, events, suspendEntry));
    nemo::coroutine::Processor::Yield();

    //事件到达
    NEMO_ASSERT(events[0] & POLLIN);
    char buf[1024]{0};
    ::read(fds[0], buf, sizeof buf);
    NEMO_LOG_DEBUG(rootLogger) << "received, buf=" << buf;
}

void Send() {
    ::sleep(2);
    char msg[] = "My name is Van";
    ::write(fds[1], msg, sizeof msg);
    NEMO_LOG_DEBUG(rootLogger) << "send finish";
}