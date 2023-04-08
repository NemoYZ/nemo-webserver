#pragma once

#include <memory>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "common/noncopyable.h"
#include "common/types.h"
#include "common/lexical_cast.h"
#include "coroutine/coroutine.h"
#include "net/socket.h"

namespace nemo {
namespace net {

struct ServerConfig {
    std::vector<String> addresses;             ///< 地址
    String id;                                 ///< id
    String type;                               ///< 服务器类型, http, https...
    String name;                               ///< 服务器名称
    String certFile;                           ///< 证书文件，可选
    String keyFile;                            ///< 密钥文件，可选
    int timeoutMillionSeconds = 1000 * 2 * 60; ///< 超时时间，可选
    bool keepAlive = false;                    ///< 长连接，可选
    bool ssl = 0;                              ///< 是否用ssl，可选

    bool isValid() const {
        return !addresses.empty() && !type.empty();
    }

    bool operator==(const ServerConfig& other) const {
        return name == other.name;
    }
};

class Server {
public:
    typedef std::shared_ptr<Server> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Server> UniquePtr; ///< 智能指针定义

public:
    Server(const coroutine::Scheduler::SharedPtr&  ioScheduler = nullptr);
    virtual ~Server();

    /**
     * @brief 绑定地址
     * @param[in] addrs 需要绑定的地址
     * @param[in] ssl 是否用ssl，true表示要用
     * @return 返回是否绑定成功
     */
    virtual bool bind(const Address*, bool ssl = false) = 0;

    /**
     * @brief 绑定地址数组
     * @param[in] addrs 需要绑定的地址数组
     * @param[in] ssl 是否用ssl，true表示要用
     * @return 绑定失败的地址
     */
    virtual bool bind(const std::vector<Address::UniquePtr>& addresses, 
            std::vector<Address*>& fails, bool ssl = false) = 0;

    /**
     * @brief 启动服务
     * @pre 需要bind成功后执行
     */
    virtual bool start() = 0;

    /**
     * @brief 停止服务
     */
    virtual void stop() = 0;

    /**
     * @brief 返回服务器名称
     */
    const String& getName() const { return config_->name; }

    /**
     * @brief 设置服务器名称
     */
    virtual void setName(StringArg name) { config_->name = name; }

    /**
     * @brief 是否停止
     */
    bool isStop() const { return stop_; }

    /**
     * @brief 返回配置
     * @return 配置 
     */
    ServerConfig* getConfig() const { return config_.get(); }

    /**
     * @brief 设置配置
     * @param[in] conf 配置
     */
    void setConfig(std::unique_ptr<ServerConfig>&& config) { 
        config_ = std::move(config); 
    }

    void setConfig(const ServerConfig& config) {
        config_ = std::make_unique<ServerConfig>(config);
    }

    /**
     * @brief 转化为字符串
     * @param prefix 前缀
     * @return 人可以读懂的字符串 
     */
    virtual String toString(StringArg prefix = "") const;
    

protected:
    coroutine::Scheduler::SharedPtr ioScheduler_;
    std::unique_ptr<ServerConfig> config_;
    std::vector<Socket::UniquePtr> sockets_;
    bool stop_;
};

} // namespace net

template<>
inline net::ServerConfig LexicalCast<net::ServerConfig, String>(const String& str) {
    YAML::Node node = YAML::Load(str);
    net::ServerConfig config;
    config.id = node["id"].as<String>(config.id);
    config.type = node["type"].as<String>(config.type);
    config.name = node["name"].as<String>(config.name);
    config.ssl = node["ssl"].as<bool>(config.ssl);
    config.certFile = node["cert_file"].as<String>(config.certFile);
    config.keyFile = node["key_file"].as<String>(config.keyFile);
    if ("tcp" == config.type || "http" == config.type) {
        config.keepAlive = node["keep_alive"].as<bool>(config.keepAlive);
        config.timeoutMillionSeconds = 
            node["timeout"].as<int>(config.timeoutMillionSeconds);
    }
    if(node["addresses"].IsDefined()) {
        for(size_t i = 0; i < node["addresses"].size(); ++i) {
            config.addresses.push_back(node["addresses"][i].as<String>());
        }
    }
    return config;
};

template<>
inline String LexicalCast<String, net::ServerConfig>(const net::ServerConfig& config) {
    YAML::Node node;
    node["id"] = config.id;
    node["type"] = config.type;
    node["name"] = config.name;
    node["ssl"] = config.ssl;
    node["cert_file"] = config.certFile;
    node["key_file"] = config.keyFile;
    if ("tcp" == config.type || "http" == config.type) {
        node["keep_alive"] = config.keepAlive;
        node["timeout"] = config.timeoutMillionSeconds;
    }
    for(auto& address : config.addresses) {
        node["addresses"].push_back(address);
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
};

} // namespace nemo