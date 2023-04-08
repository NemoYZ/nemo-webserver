#pragma once

#include <memory>

#include "net/server.h"

namespace nemo {
namespace net {

class TcpServer : public Server {
public:
    typedef std::shared_ptr<TcpServer> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<TcpServer> UniquePtr; ///< 智能指针定义

public:
    TcpServer(const coroutine::Scheduler::SharedPtr& ioScheduler = nullptr,
        const coroutine::Scheduler::SharedPtr& acceptScheduler = nullptr,
        const coroutine::Scheduler::SharedPtr& handleScheduler = nullptr);

    virtual ~TcpServer();

    /**
     * @brief 绑定地址
     * @param[in] addrs 需要绑定的地址
     * @param[in] ssl 是否用ssl，true表示要用
     * @return 返回是否绑定成功
     */
    virtual bool bind(const Address* address, bool ssl = false) override;

    /**
     * @brief 绑定地址数组
     * @param[in] addrs 需要绑定的地址数组
     * @param[in] ssl 是否用ssl，true表示要用
     * @return 绑定失败的地址
     */
    virtual bool bind(const std::vector<Address::UniquePtr>& addresses, 
            std::vector<Address*>& fails, bool ssl = false) override;

    /**
     * @brief 启动服务
     * @pre 需要bind成功后执行
     */
    virtual bool start() override;

    /**
     * @brief 停止服务
     */
    virtual void stop() override;

    /**
     * @brief 返回读取超时时间(毫秒)
     */
    uint64_t getRecvTimeout() const { return recvTimeoutMillionSeconds_; }

    /**
     * @brief 设置读取超时时间(毫秒)
     */
    void setRecvTimeout(uint64_t recvTimeoutMillionSeconds) { 
        recvTimeoutMillionSeconds_ = recvTimeoutMillionSeconds; 
    }

    /**
     * @brief 转化为字符串
     * @param prefix 前缀
     * @return 人可以读懂的字符串
     */
    virtual String toString(StringArg prefix) const override;

    /**
     * @brief 加载证书文件
     * @pre ssl为true
     * @param[in] certFile 证书文件路径
     * @param[in] keyFile 秘钥文件路径
     * @return 
     *      @retval true 加载成功
     *      @retval false 加载失败 
     */
    bool loadCertificates(StringArg certFile, StringArg keyFile);

protected:
    struct HandleWrapper {

    };

    /**
     * @brief 处理新连接的Socket类
     * @param[in] 连接客户端的socket
     */
    virtual void handleClient(Socket::SharedPtr client);

    /**
     * @brief 开始接受连接
     * @param[in] 服务端的socket
     */
    virtual void startAccept(Socket* sock);

private:
    bool bindAddress(const Address* address);

protected:
    coroutine::Scheduler::SharedPtr acceptScheduler_;
    coroutine::Scheduler::SharedPtr handleScheduler_;
    uint64_t recvTimeoutMillionSeconds_;
};

} // namespace net
} // namespace nemo