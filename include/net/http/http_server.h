#pragma once

#include "net/tcp_server.h"
#include "net/http/http_session.h"
#include "net/http/servlet.h"

namespace nemo {
namespace net {
namespace http {

class HttpServer : public TcpServer {
public:
    typedef std::shared_ptr<HttpServer> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpServer> UniquePtr; ///< 智能指针定义

    /**
     * @brief 构造函数
     * @param[in] keepalive 是否长连接
     * @param[in] worker 工作调度器
     * @param[in] ioWorker io工作调度器
     * @param[in] acceptWorker 接收连接调度器
     */
    HttpServer(bool keepalive = false,
            const coroutine::Scheduler::SharedPtr& ioScheduler = nullptr,
            const coroutine::Scheduler::SharedPtr& acceptScheduler = nullptr,
            const coroutine::Scheduler::SharedPtr&  handleScheduler = nullptr);

    /**
     * @brief 获取ServletDispatch
     */
    ServletDispatcher* getServletDispatcher() const { return dispatcher_.get(); }

    /**
     * @brief 设置ServletDispatch
     */
    void setServletDispatch(ServletDispatcher::UniquePtr&& servletDispatcher) { 
        dispatcher_ = std::move(servletDispatcher); }

protected:
    void handleClient(Socket::SharedPtr client) override;

private:
    ServletDispatcher::UniquePtr dispatcher_; ///< Servlet分发器
    bool keepalive_;                          ///< 是否支持长连接
};

} // namespace http
} // namespace net
} // namespace nemo