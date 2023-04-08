#pragma once

#include <unordered_map>

#include "net/tcp_server.h"
#include "common/types.h"
#include "common/singleton.h"
#include "coroutine/coroutine.h"

namespace nemo {

/**
 * @brief 应用程序类
 */
class Application : public Singleton<Application> {
public:
    /**
     * @brief 构造函数
     * 
     */
    Application(Token);

    /**
     * @brief 加载配置
     * @param[in] configFile 配置文件
     */
    void loadConfig(const String& configFile);

    /**
     * @brief 启动应用程序
     */
    int run(int argc, char** argv);

    /**
     * @brief 获得server
     * @param[in] name server的名字
     * @return server的指针 
     */
    net::TcpServer* getServer(const String& name);

    bool isDaemon() { return isDaemon_; }
    
private:
    /**
     * @brief 运行协程
     */
    void runTask();

    /**
     * @brief 运行的函数
     */
    int main(int argc, char** argv);

private:
    std::unordered_map<String, net::TcpServer::UniquePtr> servers_;
    bool isDaemon_;
};

} // namespace nemo