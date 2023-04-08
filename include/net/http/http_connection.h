#pragma once

#include <mutex>
#include <list>

#include "net/io/socket_stream.h"
#include "net/http/http.h"
#include "net/uri.h"

namespace nemo {
namespace net {
namespace http {

/**
 * @brief HTTP响应结果
 */
struct HttpResult {
    typedef std::shared_ptr<HttpResult> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpResult> UniquePtr; ///< 智能指针定义
    /**
     * @brief 错误码定义
     */
    enum class ErrorCode : int8_t {
        OK                      = 0,    ///< 正常
        INVALID_URL             = 1,    ///< 非法URL
        INVALID_HOST            = 2,    ///< 无法解析HOST
        CONNECT_FAILED          = 3,    ///< 连接失败
        SEND_CLOSE_BY_PEER      = 4,    ///< 连接被对端关闭
        SEND_SOCKET_ERROR       = 5,    ///< 发送请求产生Socket错误
        TIMEOUT                 = 6,    ///< 超时
        CREATE_SOCKET_ERROR     = 7,    ///< 创建Socket失败
        POOL_GET_CONNECTION     = 8,    ///< 从连接池中取连接失败
        POOL_INVALID_CONNECTION = 9,    ///< 无效的连接
    };

    /**
     * @brief 构造函数
     * @param[in] result 错误码
     * @param[in] response HTTP响应结构体
     * @param[in] error 错误描述
     */
    HttpResult(ErrorCode errCode,
            HttpResponse::UniquePtr&& res,
            StringArg errMsg);

    String toString() const;

    HttpResponse::UniquePtr response; ///< HTTP响应结构体
    String errorMessage;           ///< 错误描述
    ErrorCode errorCode;             ///< 错误码
};

class HttpConnectionPool;
/**
 * @brief HTTP客户端类
 */
class HttpConnection {
friend class HttpConnectionPool;
public:
    typedef std::shared_ptr<HttpConnection> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpConnection> UniquePtr; ///< 智能指针定义

    /**
     * @brief 发送HTTP的GET请求
     * @param[in] url 请求的url
     * @param[out] result 请求结果
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoGet(StringArg uriStr, 
                    uint64_t timeoutMillionSeconds,
                    const std::map<String, String>& headers = {}, 
                    StringArg body = "") {
        return DoRequest(HttpMethod::GET, uriStr, 
            timeoutMillionSeconds, headers, body);
    }

    /**
     * @brief 发送HTTP的GET请求
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoGet(const Uri* uri, 
                    uint64_t timeoutMillionSeconds,
                    const std::map<String, String>& headers = {}, 
                    StringArg body = "") {
        return DoRequest(HttpMethod::GET, uri, timeoutMillionSeconds, 
            headers, body);
    }

    /**
     * @brief 发送HTTP的POST请求
     * @param[in] url 请求的url
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoPost(StringArg uriStr,
                    uint64_t timeoutMillionSeconds, 
                    const std::map<String, String>& headers = {}, 
                    StringArg body = "") {
        return DoRequest(HttpMethod::POST, uriStr,
            timeoutMillionSeconds, headers, body);
    }

    /**
     * @brief 发送HTTP的POST请求
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoPost(const Uri* uri, 
                    uint64_t timeoutMillionSeconds, 
                    const std::map<String, String>& headers = {}, 
                    StringArg body = "") {
        return DoRequest(HttpMethod::POST, uri, timeoutMillionSeconds, 
            headers, body);
    }

    /**
     * @brief 发送HTTP请求
     * @param[in] method 请求类型
     * @param[in] uri 请求的uri
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoRequest(HttpMethod method, 
                        StringArg url,
                        uint64_t timeoutMillionSeconds, 
                        const std::map<String, String>& headers = {}, 
                        StringArg body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] method 请求类型
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoRequest(HttpMethod method, 
                        const Uri* uri,
                        uint64_t timeoutMillionSeconds, 
                        const std::map<String, String>& headers = {}, 
                        StringArg body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] req 请求结构体
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @return 返回HTTP结果结构体
     */
    static HttpResult::UniquePtr DoRequest(HttpRequest* req, 
                        const Uri* uri, 
                        uint64_t timeoutMillionSeconds);

    /**
     * @brief 构造函数
     * @param[in] sock Socket类
     */
    HttpConnection(Socket* sock);

    HttpConnection(Socket::UniquePtr&& sock);

    /**
     * @brief 析构函数
     */
    ~HttpConnection();

    bool isConnect() const {
        return sockStream_ && sockStream_->isConnect();
    }

    Socket* getSocket() const {
        return sockStream_->getSocket();
    }

    /**
     * @brief 接收HTTP响应
     * @param[out] response
     */
    HttpResponse::UniquePtr recvResponse();

    /**
     * @brief 发送HTTP请求
     * @param[in] req HTTP请求结构
     */
    int sendRequest(const HttpRequest* request);

private:
    io::SocketStream::UniquePtr sockStream_;
    uint64_t createTime_ = 0;   ///< 创建时间
    uint64_t request_ = 0;
};

class HttpConnectionPool {
public:
    typedef std::shared_ptr<HttpConnectionPool> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpConnectionPool> UniquePtr; ///< 智能指针定义

    static HttpConnectionPool::UniquePtr Create(StringArg uriStr,
                    StringArg vhost,
                    uint32_t maxSize,
                    uint32_t maxAliveTime,
                    uint32_t maxRequest);

    HttpConnectionPool(StringArg host,
                    StringArg vhost,
                    uint32_t port,
                    uint32_t maxSize,
                    uint32_t maxAliveTime,
                    uint32_t maxRequest,
                    bool isHttps);

    HttpConnection* getConnection();


    /**
     * @brief 发送HTTP的GET请求
     * @param[in] url 请求的url
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doGet(StringArg uriStr, 
            uint64_t timeoutMillionSeconds,
            const std::map<String, String>& headers = {}, 
            StringArg body = "") {
        return doRequest(HttpMethod::GET, uriStr,
            timeoutMillionSeconds, headers, body);
    }

    /**
     * @brief 发送HTTP的GET请求
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doGet(const Uri* uri, 
            uint64_t timeoutMillionSeconds,
            const std::map<String, String>& headers = {}, 
            StringArg body = "") {
        return doRequest(HttpMethod::GET, uri, 
            timeoutMillionSeconds, headers, body);
    }

    /**
     * @brief 发送HTTP的POST请求
     * @param[in] url 请求的url
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doPost(StringArg uriStr, 
                uint64_t timeoutMillionSeconds,
                const std::map<String, String>& headers = {}, 
                StringArg body = "") {
        return doRequest(HttpMethod::POST, uriStr, 
            timeoutMillionSeconds, headers, body);
    }

    /**
     * @brief 发送HTTP的POST请求
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doPost(const Uri* uri, 
                uint64_t timeoutMillionSeconds,
                const std::map<String, String>& headers = {}, 
                StringArg body = "") {
        return doRequest(HttpMethod::POST, uri, 
            timeoutMillionSeconds, headers, body);
    }

    /**
     * @brief 发送HTTP请求
     * @param[in] method 请求类型
     * @param[in] uri 请求的url
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doRequest(HttpMethod method, 
                StringArg uriStr, 
                uint64_t timeoutMillionSeconds,
                const std::map<String, String>& headers = {}, 
                StringArg body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] method 请求类型
     * @param[in] uri URI结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doRequest(HttpMethod method, 
                const Uri* uri, 
                uint64_t timeoutMillionSeconds,
                const std::map<String, String>& headers = {}, 
                StringArg body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] req 请求结构体
     * @param[in] timeoutMillionSeconds 超时时间(毫秒)
     * @return 返回HTTP结果结构体
     */
    HttpResult::UniquePtr doRequest(HttpRequest* request, 
                uint64_t timeoutMillionSeconds);

private:
    std::mutex mutex_;
    String host_;                               ///< host
    String vhost_;                              ///< virtual host
    uint32_t port_;                             ///< 端口
    uint32_t maxSize_;                          ///< 最大连接数
    uint32_t maxAliveTime_;                     ///< 存活时间
    uint32_t maxRequest_;                       ///< 最大请求数
    std::list<HttpConnection::UniquePtr> connections_;  ///< http连接
    bool isHttps_;                              ///< 是否为https
};

} // namespace http
} // namespace net
} // namespace nemo
