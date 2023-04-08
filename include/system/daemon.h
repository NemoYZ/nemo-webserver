#pragma once

#include <functional>

#include "common/types.h"
#include "common/singleton.h"

namespace nemo {

/**
 * @brief 进程信息类
 */
class ProcessInfo : public Singleton<ProcessInfo> {
public:
    /**
     * @brief 默认构造
     */
    ProcessInfo(Token);

    /**
     * @brief 更新pid
     */
    inline void updatePid() { pid_ = getpid(); }

    /**
     * @brief 更新启动时间
     */
    inline void updateStartTime() { startTime_ = ::time(nullptr); }

    /**
     * @brief 更新
     * @param[in] parentPid 父进程的pid
     * @param[in] parentStartTime 父进程启动时间
     */
    void update(pid_t parentPid, time_t parentStartTime);

    /**
     * @brief 重启之后调用
     */
    void onRestart() { ++restartCount_; }

    /**
     * @brief 获得pid
     * @return pid
     */
    pid_t getPid() const { return pid_; }

    /**
     * @brief 获得父进程id
     * @return 父进程id
     */
    pid_t getParentPid() const { return parentPid_; }

    /**
     * @brief 获得父进程启动时间
     * @return 父进程启动时间
     */
    time_t getParentStartTime() const { return parentStartTime_; }

    /**
     * @brief 获得本进程启动时间
     * @return 本进程启动时间
     */
    time_t getStartTime() const { return startTime_; }

    /**
     * @brief 获得重启次数
     * @return 重启次数
     */
    uint32_t getRestartCount() const { return restartCount_; }

    /**
     * @brief 转为字符串
     * @return 一个人类可以读懂的字符串
     */
    String toString() const;

private:
    pid_t parentPid_;           ///< 父进程id
    pid_t pid_;                 ///< 主进程id
    time_t parentStartTime_;    ///< 父进程启动时间
    time_t startTime_;          ///< 主进程启动时间
    uint32_t restartCount_;     ///< 主进程重启的次数
};

/**
 * @brief 创建守护进程
 * @param[in]  func 守护进程调用的函数
 * @param[in] argc 守护进程参数个数
 * @param[in] argv 守护进程参数
 * @return func返回值
 */
int CreateDaemon(std::function<int(int, char**)> func, int argc, char** argv);

} // namespace nemo