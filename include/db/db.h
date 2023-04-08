#pragma once

#include <unordered_map>
#include <shared_mutex>
#include <sstream>

#include <yaml-cpp/yaml.h>
#include <soci/soci.h>

#include "common/lexical_cast.h"
#include "common/types.h"
#include "common/singleton.h"

namespace nemo {
namespace db {

struct DbConfig {
    String name;
    String dbname;
    String user;
    String password;
    String host;
    int32_t port{-1};
    String sslca;
    String sslcert;
    String charset="uft8";
    int connections{1};
    int connectTimeout{-1};
    int readTimeout{-1};
    int writeTimeout{-1};
    bool reconnect{false};

    String toConnectParameter();
    bool operator==(const DbConfig& other) const {
        return name == other.name;
    }
};

class DbManager : public Singleton<DbManager> {
public:
    DbManager(Token);
    void connect(const String& dbName, const String& connectParam, size_t connectionNumber);
    soci::session* getSession(const String& dbName, size_t index);
    soci::session* getSessionAny(const String& dbName);

private:
    std::unordered_map<String, soci::connection_pool> pools_;
    std::shared_mutex mutex_;
};

} // namespace db

template<>
inline db::DbConfig LexicalCast<db::DbConfig, String>(const String& yamlStr) {
    YAML::Node node = YAML::Load(yamlStr);
    db::DbConfig config;
    config.name = node["name"].as<String>();
    config.dbname = node["dbname"].as<String>();
    config.user = node["user"].as<String>();
    config.password = node["password"].as<String>();
    config.connections = node["connections"].as<int>();
    YAML::Node tmpNode = node["host"];
    if (tmpNode.IsDefined()) {
        config.host = tmpNode.as<String>();
        tmpNode = node["port"];
        config.port = tmpNode.as<int32_t>();
    }
    
    tmpNode = node["sslca"];
    if (tmpNode.IsDefined()) {
        config.sslca = tmpNode.as<String>();
        tmpNode = node["sslcert"];
        config.sslcert = tmpNode.as<String>();
    }
    
    tmpNode = node["charset"];
    if (tmpNode.IsDefined()) {
        config.charset = tmpNode.as<String>();
    }
    tmpNode = node["connect_timeout"];
    if (tmpNode.IsDefined()) {
        config.connectTimeout = node.as<int>();
    } else {
        config.connectTimeout = -1;
    }
    tmpNode = node["read_timeout"];
    if (tmpNode.IsDefined()) {
        config.readTimeout = node.as<int>();
    } else {
        config.readTimeout = -1;
    }
    tmpNode = node["write_timeout"];
    if (tmpNode.IsDefined()) {
        config.writeTimeout = node.as<int>();
    } else {
        config.writeTimeout = -1;
    }

    return config;
}

template<>
inline String LexicalCast<String, db::DbConfig>(const db::DbConfig& config) {
    YAML::Node node;
    node["name"] = config.name;
    node["dbname"] = config.dbname;
    node["user"] = config.user;
    node["password"] = config.password;
    node["connections"] = config.connections;
    if (!config.host.empty()) {
        node["host"] = config.host;
        node["port"] = config.port;
    }
    if (!config.sslca.empty()) {
        node["sslca"] = config.sslca;
        node["sslcert"] = config.sslcert;
    }
    if (!config.charset.empty()) {
        node["charset"] = config.charset;
    }
    if (config.connectTimeout > 0) {
        node["connect_timeout"] = config.connectTimeout;
    }
    if (config.readTimeout > 0) {
        node["read_timeout"] = config.readTimeout;
    }
    if (config.writeTimeout > 0) {
        node["write_timeout"] = config.writeTimeout;
    }

    return (std::stringstream() << node).str();
}

} // namespace nemo