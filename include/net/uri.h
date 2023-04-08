/*
 *   foo://user@baidu.com:80/over/there?name=ferret#fragment
 *   \_/   \_______________/\_________/ \_________/ \_____/
 *    |            |             |           |         |
 *  scheme     authority        path       query   fragment
*/

#pragma once

#include <stdint.h>

#include <memory>

#include "net/address.h"
#include "common/types.h"

namespace nemo {
namespace net {

class Uri {
public:
    typedef std::shared_ptr<Uri> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Uri> UniquePtr; ///< 智能指针定义

    /**
     * @brief 创建Uri对象
     * @param[in] uriStr uri字符串
     * @param[out] uri 
     * @return 解析成功返回Uri对象否则返回nullptr
     */
    static Uri::UniquePtr Create(StringArg uriStr);

    /**
     * @brief 构造函数
     */
    Uri();

    /**
     * @brief 返回scheme
     */
    const String& getScheme() const { return scheme_; }

    /**
     * @brief 返回用户信息
     */
    const String& getUserinfo() const { return userInfo_; }

    /**
     * @brief 返回host
     */
    const String& getHost() const { return host_; }

    /**
     * @brief 返回路径
     */
    const String& getPath() const;

    /**
     * @brief 返回查询条件
     */
    const String& getQuery() const { return query_; }

    /**
     * @brief 返回fragment
     */
    const String& getFragment() const { return fragment_; }

    /**
     * @brief 返回端口
     */
    uint16_t getPort() const;

    /**
     * @brief 设置scheme
     * @param scheme scheme
     */
    void setScheme(StringArg scheme) { scheme_ = scheme; }

    /**
     * @brief 设置用户信息
     * @param userInfo 用户信息
     */
    void setUserInfo(StringArg userInfo) { userInfo_ = userInfo; }

    /**
     * @brief 设置host信息
     * @param host host
     */
    void setHost(StringArg host) { host_ = host; }

    /**
     * @brief 设置路径
     * @param path 路径
     */
    void setPath(StringArg path) { path_ = path; }

    /**
     * @brief 设置查询条件
     * @param query
     */
    void setQuery(StringArg query) { query_ = query; }

    /**
     * @brief 设置fragment
     * @param fragment fragment
     */
    void setFragment(StringArg fragment) { fragment_ = fragment; }

    /**
     * @brief 设置端口号
     * @param port 端口
     */
    void setPort(uint16_t port) { port_ = port; }

    /**
     * @brief 序列化到输出流
     * @param os 输出流
     * @return 输出流
     */
    std::ostream& dump(std::ostream& os) const;

    /**
     * @brief 转成字符串
     */
    String toString() const;

    /**
     * @brief 获取Address
     */
    IpAddress::UniquePtr createIpAddress() const;

private:

    /**
     * @brief 是否默认端口
     */
    bool isDefaultPort() const;

private:
    String scheme_;       ///< schema
    String userInfo_;     ///< 用户信息
    String host_;         ///< host
    String path_;         ///< 路径
    String query_;        ///< 查询参数
    String fragment_;     ///< fragment
    int32_t port_;             ///< 端口
};

} // namespace net
} // namespace nemo