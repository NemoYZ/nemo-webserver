#include "db/db.h"

namespace nemo {
namespace db {

static void StringAppend(String& target, const String& str, const String& tail = " ") {
    target += str;
    target += tail;
}

String DbConfig::toConnectParameter() {
    String str;
    StringAppend(str, "dbname=" + dbname);
    StringAppend(str, "user=" + user);
    StringAppend(str, "password=" + password);
    if (!host.empty()) {
        StringAppend(str, "host=" + host);
        StringAppend(str, "port=" + LexicalCast<String>(port));
    }
    if (!sslca.empty()) {
        StringAppend(str, "sslca=" + sslca);
        StringAppend(str, "sslcert=" + sslcert);
    }
    if (!charset.empty()) {
        StringAppend(str, "charset=" + charset);
    }
    if (reconnect) {
        StringAppend(str, "reconnect=1");
    }
    if (connectTimeout != -1) {
        StringAppend(str, "connect_timeout=" + LexicalCast<String>(connectTimeout));
    }
    if (readTimeout != -1) {
        StringAppend(str, "read_timeout=" + LexicalCast<String>(readTimeout));
    }
    if (writeTimeout != -1) {
        StringAppend(str, "write_timeout=" + LexicalCast<String>(writeTimeout));
    }
    // trim last blank
    str.pop_back();

    return str;
}

DbManager::DbManager(Token) {
}

void DbManager::connect(const String& dbName, const String& connectParam, size_t connectionNumber) {
    {
        std::shared_lock<std::shared_mutex> sharedLock(mutex_);
        if (pools_.find(dbName) != pools_.end()) {
            return;
        }
    }
    std::lock_guard<std::shared_mutex> lockGuard(mutex_);
    auto myPair = pools_.emplace(dbName, connectionNumber);
    soci::connection_pool& pool = (myPair.first->second);
    for (size_t i = 0; i < connectionNumber; ++i) {
        soci::session& s = pool.at(i);
        s.open(dbName, connectParam);
    }
}

soci::session* DbManager::getSession(const String& dbName, size_t index) {
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    auto iter = pools_.find(dbName);
    if (iter != pools_.end()) {
        return &iter->second.at(index);
    }
    return nullptr;
}

soci::session* DbManager::getSessionAny(const String& dbName) {
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    auto iter = pools_.find(dbName);
    if (iter != pools_.end()) {
        size_t pos = iter->second.lease();
        return &iter->second.at(pos);
    }
    return nullptr;
}

} // namespace db
} // namespace nemo