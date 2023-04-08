#pragma once

#include <memory>
#include <map>

#include "net/http/http_method.h"
#include "net/http/http_status.h"
#include "common/types.h"
#include "common/lexical_cast.h"
#include "util/case_insensitive_compare.h"

namespace nemo {
namespace net {
namespace http {

class HttpSession;

enum class HttpVersion : int8_t {
    HTTP10 = 0x10,
    HTTP11 = 0x11,
    HTTP20 = 0x20 //Not support temporarily
};

/**
 * @brief 获取Map中的key值,并转成对应类型,返回是否成功
 * @param[in] m Map数据结构
 * @param[in] key 关键字
 * @param[out] val 保存转换后的值
 * @param[in] defaultVal 默认值
 * @return
 *      @retval true 转换成功, val 为对应的值
 *      @retval false 不存在或者转换失败 val = defaultVal
 */
template<typename MapType, typename T>
bool CheckGetAs(const MapType& m, const std::string& key, T& val, const T& defaultVal = T()) {
    auto iter = m.find(key);
    if(m.end() == iter) {
        val = defaultVal;
        return false;
    }
    try {
        val = boost::lexical_cast<T>(iter->second);
        return true;
    } catch (...) {
        val = defaultVal;
    }
    return false;
}

/**
 * @brief 获取Map中的key值,并转成对应类型
 * @param[in] m Map数据结构
 * @param[in] key 关键字
 * @param[in] defaultVal 默认值
 * @return 如果存在且转换成功返回对应的值,否则返回默认值
 */
template<typename MapType, typename T>
T GetAs(const MapType& m, const std::string& key, const T& defaultVal = T()) {
    auto iter = m.find(key);
    if(m.end() == iter) {
        return defaultVal;
    }
    try {
        return boost::lexical_cast<T>(iter->second);
    } catch (...) {
    }
    return defaultVal;
}

class HttpResponse; //前置声明

/**
 * @brief HTTP请求结构
 */
class HttpRequest {
friend class HttpSession;
public:
    typedef std::shared_ptr<HttpRequest> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpRequest> UniquePtr; ///< 智能指针定义
    typedef std::map<String, String, string_util::CaseInsensitiveLess> MapType; ///< MapType定义

    /**
     * @brief 构造函数
     * @param[in] version 版本
     * @param[in] close 是否keepalive
     */
    HttpRequest(HttpVersion version = HttpVersion::HTTP11, bool close = true);

    bool createResponse(std::unique_ptr<HttpResponse>& response);

    /**
     * @brief 返回HTTP方法
     */
    HttpMethod getMethod() const { return method_; }

    /**
     * @brief 返回HTTP版本
     */
    HttpVersion getVersion() const { return version_; }

    /**
     * @brief 返回HTTP请求的路径
     */
    const String& getPath() const { return path_; }

    /**
     * @brief 返回HTTP请求的查询参数
     */
    const String& getQuery() const { return query_; }

    /**
     * @brief 返回HTTP请求的消息体
     */
    const String& getBody() const { return body_; }

    /**
     * @brief 返回HTTP请求的消息头MAP
     */
    const MapType& getHeaders() const { return headers_; }

    /**
     * @brief 返回HTTP请求的参数MAP
     */
    const MapType& getParams() const { return params_; }

    /**
     * @brief 返回HTTP请求的cookie MAP
     */
    const MapType& getCookies() const { return cookies_; }

    /**
     * @brief 设置HTTP请求的方法名
     * @param[in] method HTTP请求
     */
    void setMethod(HttpMethod method) { method_ = method; }

    /**
     * @brief 设置HTTP请求的协议版本
     * @param[in] version 协议版本0x11, 0x10
     */
    void setVersion(HttpVersion version) { version_ = version; }

    /**
     * @brief 设置HTTP请求的路径
     * @param[in] path 请求路径
     */
    void setPath(StringArg path) { path_ = path; }

    /**
     * @brief 设置HTTP请求的查询参数
     * @param[in] query 查询参数
     */
    void setQuery(StringArg query) { query_ = query; }

    /**
     * @brief 设置HTTP请求的Fragment
     * @param[in] fragment fragment
     */
    void setFragment(StringArg fragment) { fragment_ = fragment; }

    /**
     * @brief 设置HTTP请求的消息体
     * @param[in] body 消息体
     */
    void setBody(StringArg body) { body_ = body; }

    void setBody(String&& body) { body_ = std::move(body); }

    /**
     * @brief 是否自动关闭
     */
    bool isClose() const { return close_; }

    /**
     * @brief 设置是否自动关闭
     * @param[in] flag 为true表示持续型连接，为false表示非持续型连接
     */
    void setClose(bool flag) { close_ = flag; }

    /**
     * @brief 是否websocket
     */
    bool isWebsocket() const { return webSocket_; }

    /**
     * @brief 设置是否websocket
     * @param[in] flag 为true表示是websocket，为false表示不是websocket
     */
    void setWebsocket(bool flag) { webSocket_ = flag; }

    /**
     * @brief 设置HTTP请求的头部MAP
     * @param[in] headers map
     */
    void setHeaders(const MapType& headers) { headers_ = headers; }

    void setHeaders(MapType&& headers) { headers_ = std::move(headers); }

    /**
     * @brief 设置HTTP请求的参数MAP
     * @param[in] param map
     */
    void setParams(const MapType& param) { params_ = param; }

    void setParams(MapType&& param) { params_ = std::move(param); }

    /**
     * @brief 设置HTTP请求的Cookie MAP
     * @param[in] cookies map
     */
    void setCookies(const MapType& cookies) { cookies_ = cookies; }

    void setCookies(MapType&& cookies) { cookies_ = std::move(cookies); }

    /**
     * @brief 获取HTTP请求的头部参数
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在则返回对应值,否则返回默认值
     */
    String getHeader(const String& key, const String& defaultVal = "") const;

    /**
     * @brief 获取HTTP请求的请求参数
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在则返回对应值,否则返回默认值
     */
    String getParam(const String& key, const String& defaultVal = "");

    /**
     * @brief 获取HTTP请求的Cookie参数
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在则返回对应值,否则返回默认值
     */
    String getCookie(const String& key, const String& defaultVal = "");

    /**
     * @brief 设置HTTP请求的头部参数
     * @param[in] key 关键字
     * @param[in] val 值
     */
    void setHeader(const String& key, const String& val);

    void setHeader(const String& key, String&& val);

    /**
     * @brief 设置HTTP请求的请求参数
     * @param[in] key 关键字
     * @param[in] val 值
     */
    void setParam(const String& key, const String& val);

    void setParam(const String& key, String&& val);

    /**
     * @brief 设置HTTP请求的Cookie参数
     * @param[in] key 关键字
     * @param[in] val 值
     */
    void setCookie(const String& key, const String& val);

    void setCookie(const String& key, String&& val);

    /**
     * @brief 删除HTTP请求的头部参数
     * @param[in] key 关键字
     */
    void delHeader(const String& key);

    /**
     * @brief 删除HTTP请求的请求参数
     * @param[in] key 关键字
     */
    void delParam(const String& key);

    /**
     * @brief 删除HTTP请求的Cookie参数
     * @param[in] key 关键字
     */
    void delCookie(const String& key);

    /**
     * @brief 判断HTTP请求的头部参数是否存在
     * @param[in] key 关键字
     * @param[out] val 如果存在,val非空则赋值
     * @return 是否存在
     */
    bool hasHeader(const String& key, String* val = nullptr);

    /**
     * @brief 判断HTTP请求的请求参数是否存在
     * @param[in] key 关键字
     * @param[out] val 如果存在,val非空则赋值
     * @return 是否存在
     */
    bool hasParam(const String& key, String* val = nullptr);

    /**
     * @brief 判断HTTP请求的Cookie参数是否存在
     * @param[in] key 关键字
     * @param[out] val 如果存在,val非空则赋值
     * @return 是否存在
     */
    bool hasCookie(const String& key, String* val = nullptr);

    /**
     * @brief 检查并获取HTTP请求的头部参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[out] val 返回值
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回true,否则失败val=defaultVal
     */
    template<typename T>
    bool checkGetHeaderAs(const String& key, T& val, const T& defaultVal = T()) {
        return CheckGetAs(headers_, key, val, defaultVal);
    }

    /**
     * @brief 获取HTTP请求的头部参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回对应的值,defaultVal
     */
    template<typename T>
    T getHeaderAs(const String& key, const T& defaultVal = T()) {
        return GetAs(headers_, key, defaultVal);
    }

    /**
     * @brief 检查并获取HTTP请求的请求参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[out] val 返回值
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回true,否则失败val=defaultVal
     */
    template<typename T>
    bool checkGetParamAs(const String& key, T& val, const T& defaultVal = T()) {
        initQueryParam();
        initBodyParam();
        return CheckGetAs(params_, key, val, defaultVal);
    }

    /**
     * @brief 获取HTTP请求的请求参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回对应的值,否则返回defaultVal
     */
    template<typename T>
    T getParamAs(const String& key, const T& defaultVal = T()) {
        initQueryParam();
        initBodyParam();
        return GetAs(params_, key, defaultVal);
    }

    /**
     * @brief 检查并获取HTTP请求的Cookie参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[out] val 返回值
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回true,否则失败val=defaultVal
     */
    template<typename T>
    bool checkGetCookieAs(const String& key, T& val, const T& defaultVal = T()) {
        initCookies();
        return CheckGetAs(cookies_, key, val, defaultVal);
    }

    /**
     * @brief 获取HTTP请求的Cookie参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回对应的值,否则返回defaultVal
     */
    template<typename T>
    T getCookieAs(const String& key, const T& defaultVal = T()) {
        initCookies();
        return GetAs(cookies_, key, defaultVal);
    }

    /**
     * @brief 序列化输出到流中
     * @param[in, out] os 输出流
     * @return 输出流
     */
    std::ostream& dump(std::ostream& os) const;

    /**
     * @brief 转成字符串类型
     * @return 字符串
     */
    String toString() const;

private:
    void init();
    void initParam();
    void initQueryParam();
    void initBodyParam();
    void initCookies();

private:
    HttpMethod method_;        //HTTP方法
    HttpVersion version_;     //HTTP版本
    bool close_;               //是否为长连接
    bool webSocket_;           //是否为websocket
    uint8_t parserParamFlag_;
    String path_;         //请求路径
    String query_;        //请求参数
    String fragment_;     //请求fragment
    String body_;         //请求消息体
    MapType headers_;          //请求头部Map
    MapType params_;           //请求参数Map
    MapType cookies_;          //请求Cookie Map
};

/**
 * @brief HTTP响应结构体
 */
class HttpResponse {
public:
    typedef std::shared_ptr<HttpResponse> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<HttpResponse> UniquePtr; ///< 智能指针定义
    typedef std::map<String, String, string_util::CaseInsensitiveLess> MapType; ///< MapType 定义

    /**
     * @brief 构造函数
     * @param[in] version 版本
     * @param[in] close 是否自动关闭
     */
    HttpResponse(HttpVersion version = HttpVersion::HTTP11, bool close = true);

    /**
     * @brief 返回响应状态
     * @return 请求状态
     */
    HttpStatus getStatus() const { return status_; }

    /**
     * @brief 返回响应版本
     * @return 版本
     */
    HttpVersion getVersion() const { return version_; }

    /**
     * @brief 返回响应消息体
     * @return 消息体
     */
    const String& getBody() const { return body_; }

    /**
     * @brief 返回响应原因
     */
    const String& getReason() const { return reason_; }

    /**
     * @brief 返回响应头部MAP
     * @return MAP
     */
    const MapType& getHeaders() const { return headers_; }

    /**
     * @brief 设置响应状态
     * @param[in] status 响应状态
     */
    void setStatus(HttpStatus status) { status_ = status; }

    /**
     * @brief 设置响应版本
     * @param[in] version 版本
     */
    void setVersion(HttpVersion version) { version_ = version; }

    /**
     * @brief 设置响应消息体
     * @param[in] body 消息体
     */
    void setBody(StringArg body) { body_ = body; }

    void setBody(String&& body) { body_ = std::move(body); }

    /**
     * @brief 设置响应原因
     * @param[in] reason 原因
     */
    void setReason(const String& reason) { reason_ = reason; }

    void setReason(String&& reason) { reason_ = std::move(reason); }

    /**
     * @brief 设置响应头部MAP
     * @param[in] headers MAP
     */
    void setHeaders(const MapType& headers) { headers_ = headers; }

    void setHeaders(MapType&& headers) { headers_ = std::move(headers); }

    /**
     * @brief 是否自动关闭
     */
    bool isClose() const { return close_; }

    /**
     * @brief 设置是否自动关闭
     * @param [in] flag 为true表示持续型连接，为false表示非持续型连接
     */
    void setClose(bool flag) { close_ = flag; }

    /**
     * @brief 是否websocket
     */
    bool isWebsocket() const { return webSocket_; }

    /**
     * @brief 设置是否websocket
     * @param [in] flag 为true表示是websocket，为false表示不是websocket
     */
    void setWebsocket(bool flag) { webSocket_ = flag; }

    /**
     * @brief 获取响应头部参数
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在返回对应值,否则返回defaultVal
     */
    String getHeader(const String& key, const String& defaultVal = "") const;

    /**
     * @brief 设置响应头部参数
     * @param[in] key 关键字
     * @param[in] val 值
     */
    void setHeader(const String& key, const String& val);

    void setHeader(const String& key, String&& val);

    /**
     * @brief 删除响应头部参数
     * @param[in] key 关键字
     */
    void delHeader(const String& key);

    /**
     * @brief 检查并获取响应头部参数
     * @tparam T 值类型
     * @param[in] key 关键字
     * @param[out] val 值
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回true,否则失败val=defaultVal
     */
    template<class T>
    bool checkGetHeaderAs(const String& key, T& val, const T& defaultVal = T()) {
        return CheckGetAs(headers_, key, val, defaultVal);
    }

    /**
     * @brief 获取响应的头部参数
     * @tparam T 转换类型
     * @param[in] key 关键字
     * @param[in] defaultVal 默认值
     * @return 如果存在且转换成功返回对应的值,否则返回defaultVal
     */
    template<class T>
    T getHeaderAs(const String& key, const T& defaultVal = T()) {
        return GetAs(headers_, key, defaultVal);
    }

    /**
     * @brief 序列化输出到流
     * @param[in, out] os 输出流
     * @return 输出流
     */
    std::ostream& dump(std::ostream& os) const;

    /**
     * @brief 转成字符串
     */
    String toString() const;

    void setRedirect(const String& uri);
    void setCookie(const String& key, const String& val,
                   time_t expired = 0, const String& path = "",
                   const String& domain = "", bool secure = false);
private:
    HttpStatus status_;                     ///< 响应状态
    HttpVersion version_;                  ///< 版本
    bool close_;                            ///< 是否自动关闭
    bool webSocket_;                        ///< 是否为websocket
    String body_;                      ///< 响应消息体
    String reason_;                    ///< 响应原因
    MapType headers_;                       ///< 响应头部MAP
    std::vector<String> cookies_;      ///< cookies
};

/**
 * @brief 流式输出HttpRequest
 * @param[in, out] os 输出流
 * @param[in] request HTTP请求
 * @return 输出流
 */
std::ostream& operator<<(std::ostream& os, const HttpRequest& request);

/**
 * @brief 流式输出HttpResponse
 * @param[in, out] os 输出流
 * @param[in] response HTTP响应
 * @return 输出流
 */
std::ostream& operator<<(std::ostream& os, const HttpResponse& response);

} // namepsace http
} // namespace net

template<>
inline net::http::HttpMethod 
LexicalCast<net::http::HttpMethod, StringArg>(const StringArg& str) {
    return net::http::String2HttpMethod(str);
}

template<>
inline net::http::HttpMethod
LexicalCast<net::http::HttpMethod, String>(const String& str) {
    return net::http::String2HttpMethod(str);
}

template<>
inline const char*
LexicalCast<const char*, net::http::HttpMethod>(const net::http::HttpMethod& method) {
    return net::http::HttpMethod2String(method);
}

} // namespace nemo