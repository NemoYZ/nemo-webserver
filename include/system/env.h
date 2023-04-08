#pragma once

#include "common/types.h"
#include "common/singleton.h"

namespace nemo {

/**
 * @brief 环境变量类封装
 * @attention 加载system.yaml文件的配置
 */
class Env : public Singleton<Env> {
public:
    /**
     * @brief 默认构造
     */
    Env(Token);
    
    /**
     * @brief 获得可执行文件的路径
     * @return 可执行文件的路径
     */
    const String& getExecDir() const { return execDir_; }

    /**
     * @brief 获得当前工作路径
     * @return 当前工作路径
     */
    const String& getCurrentWorkDir() const { return currWorkDir_; }

    /**
     * @brief 获得工作路径
     * @return 工作路径
     */
    const String& getWorkDir() const { return workDir_; }

    pid_t getPid() const { return getpid(); }

    const String& getHostName() const { return hostName_; }

private:
    String execDir_;     ///< 可执行文件的路径
    String currWorkDir_; ///< 启动程序的路径
    String workDir_;     ///< 程序工作路径
    String hostName_;    ///< 主机名
};

} //namespace nemo