#pragma once

#include "net/http/http.h"
#include "net/io/socket_stream.h"

namespace nemo {
namespace net {
namespace http {

/**
 * @brief HTTPSession封装
 */
class HttpSession {
public:
    typedef std::shared_ptr<HttpSession> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpSession> UniquePtr; ///< 智能指针定义

    /**
     * @brief 构造函数
     * @param[in] sock Socket类型
     * @param[in] owner 是否托管
     */
    HttpSession(Socket* sock);

    HttpSession(Socket::UniquePtr&& sock);

    /**
     * @brief 接收HTTP请求
     */
    HttpRequest::UniquePtr recvRequest();

    /**
     * @brief 发送HTTP响应
     * @param[in] response HTTP响应
     * @return >0 发送成功
     *         =0 对方关闭
     *         <0 Socket异常
     */
    int sendResponse(const HttpResponse* response);

private:
    io::SocketStream::UniquePtr sockStream_;
};

} // namespace http
} // namespace net
} // namespace nemo