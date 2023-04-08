#include "system/daemon.h"

#include <sys/types.h>
#include <sys/wait.h>

#include "util/util.h"
#include "log/log.h"
#include "common/config.h"

namespace nemo {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");
static ConfigVar<uint32_t>* daemonRestartInterval
    = Config::Lookup("daemon.restart_interval", static_cast<uint32_t>(5), "daemon restart interval");

ProcessInfo::ProcessInfo(Token) {
}

void ProcessInfo::update(pid_t parentPid, time_t parentStartTime) {
    parentPid_ = parentPid;
    parentStartTime_ = parentStartTime;
    updatePid();
    updateStartTime();
}

std::string ProcessInfo::toString() const {
    std::stringstream ss;
    ss << "[ProcessInfo parent pid=" << parentPid_
       << " pid=" << pid_
       << " parent start time=" << Time2Str(parentStartTime_, "%Y-%m-%d %T")
       << " start time=" << Time2Str(startTime_, "%Y-%m-%d %T")
       << " restart count=" << restartCount_ << "]";
    return ss.str();
}

int CreateDaemon(std::function<int(int, char**)> func, int argc, char** argv) {
    int result = ::daemon(1, 0);
    if (result) {
        NEMO_LOG_ERROR(systemLogger) << "create daemon faild, returned " << result
                << " errno = " << errno << " errstr = " << strerror(errno);
        return -1;
    }

    ProcessInfo::GetInstance().update(::getpid(), ::time(nullptr));

    while (true) {
        pid_t childPid = ::fork();
        if (0 == childPid) { //子进程
            ProcessInfo::GetInstance().updatePid();
            ProcessInfo::GetInstance().updateStartTime();
            NEMO_LOG_INFO(systemLogger) << "process with pid " << ProcessInfo::GetInstance().getPid()
                    << " start";
            return func(argc, argv);
        } else if (-1 == childPid) { //fork失败
            NEMO_LOG_ERROR(systemLogger) << "fork fail return=" << childPid
                << " errno=" << errno << " errstr=" << strerror(errno);
        } else { //父进程
            int status = 0;
            ::waitpid(childPid, &status, 0);
            if(status) {
                NEMO_LOG_ERROR(systemLogger) << "child crash pid=" << childPid
                        << " status=" << status;
            } else {
                NEMO_LOG_INFO(systemLogger) << "child finished pid=" << childPid;
                break;
            }
            ProcessInfo::GetInstance().onRestart();
            ::sleep(daemonRestartInterval->getValue()); //留点时间来回收子进程的资源
        }
    }
    return 0;
}

} // namespace nemo