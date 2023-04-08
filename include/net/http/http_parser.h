#pragma once

#include "net/http/http.h"
#include "net/http/http11_parser.h"
#include "net/http/http_client_parser.h"

namespace nemo {
namespace net {
namespace http {

class HttpConnection;
class HttpSession;

enum class HttpParserError : int16_t {
    OK              = 0,
    INVALID_METHOD  = 1000,
    INVALID_VERSION = 1001,
    INVALID_FIELD   = 1002,
};

/**
 * @brief HTTP请求解析类
 */
class HttpRequestParser {
friend class HttpSession;
public:
    typedef std::shared_ptr<HttpRequestParser> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpRequestParser> UniquePtr; ///< 智能指针定义

public:
    /**
     * @brief 返回HttpRequest协议解析的缓存大小
     */
    static size_t GetHttpRequestBufferSize();

    /**
     * @brief 返回HttpRequest协议的最大消息体大小
     */
    static size_t GetHttpRequestMaxBodySize();

    /**
     * @brief 构造函数
     */
    HttpRequestParser();

    /**
     * @brief 解析协议
     * @param[in, out] data 协议文本内存
     * @param[in] len 协议文本内存长度
     * @return 返回实际解析的长度,并且将已解析的数据移除
     */
    size_t execute(char* data, size_t len);

    /**
     * @brief 是否解析完成
     * @return 是否解析完成
     */
    int isFinished();

    void setError(HttpParserError err) { error_ = err; }

    /**
     * @brief 是否有错误
     * @return 是否有错误
     */
    HttpParserError getError() const { return error_; }

    bool hasError() {
        return error_ != HttpParserError::OK || 
            http_parser_has_error(&parser_);
    }

    /**
     * @brief 返回HttpRequest结构体
     */
    HttpRequest* getRequest() const { return request_.get(); }

    /**
     * @brief 获取消息体长度
     */
    size_t getContentLength();

    /**
     * @brief 获取http_parser结构体
     */
    const http_parser& getParser() const { return parser_; }

private:
    http_parser parser_;             ///< http_parser
    HttpRequest::UniquePtr request_; ///< HttpRequest结构 
    HttpParserError error_;              
};

/**
 * @brief Http响应解析结构体
 */
class HttpResponseParser {
friend class HttpConnection;
public:
    typedef std::shared_ptr<HttpResponseParser> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpResponseParser> UniquePtr; ///< 智能指针定义

public:
    /**
     * @brief 返回HTTP响应解析缓存大小
     */
    static size_t GetHttpResponseBufferSize();

    /**
     * @brief 返回HTTP响应最大消息体大小
     */
    static size_t GetHttpResponseMaxBodySize();

    /**
     * @brief 构造函数
     */
    HttpResponseParser();

    /**
     * @brief 解析HTTP响应协议
     * @param[in, out] data 协议数据内存
     * @param[in] len 协议数据内存大小
     * @param[in] chunck 是否在解析chunck
     * @return 返回实际解析的长度,并且移除已解析的数据
     */
    size_t execute(char* data, size_t len, bool chunck);

    /**
     * @brief 是否解析完成
     */
    int isFinished() {
        return httpclient_parser_finish(&parser_);
    }

    void setError(HttpParserError err) { error_ = err; }

    HttpParserError getError();

    bool hasError() {
        return error_ != HttpParserError::OK || 
            httpclient_parser_has_error(&parser_);
    }

    /**
     * @brief 返回HttpResponse
     */
    HttpResponse* getResponse() const { return response_.get();}

    /**
     * @brief 获取消息体长度
     */
    size_t getContentLength() {
        return response_->getHeaderAs<size_t>("content-length", 0);
    }

    /**
     * @brief 返回httpclient_parser
     */
    const httpclient_parser& getParser() const { return parser_;}

private:
    httpclient_parser parser_;          ///< httpclient_parser
    HttpResponse::UniquePtr response_;  ///< HttpResponse
    HttpParserError error_;
};

} // namespace http
} // namespace net
} // namespace nemo