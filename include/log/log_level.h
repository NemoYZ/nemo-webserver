#pragma once

#include <memory>

#include "common/lexical_cast.h"
#include "common/types.h"

namespace nemo {

/**
 * @brief 日志级别枚举
 */
enum LogLevel : uint8_t {
    ALL   = 0,  ///< ALL级别
    DEBUG = 1,  ///< DEBUG级别
    INFO  = 2,  ///< INFO级别
    WARN  = 3,  ///< WARN级别
    ERROR = 4,  ///< ERROR级别
    FATAL = 5   ///< FATAL级别
};
    
/**
 * @brief 将日志级别转化为字符串
 * @param[in] level 日志级别
 */
const char* LogLevel2String(LogLevel level);

/**
 * @brief 将字符串转化为日志级别
 * @param[in] str 日志级别字符串
 */
LogLevel String2LogLevel(StringArg str);

template<>
inline const char* LexicalCast<const char*, LogLevel>(const LogLevel& level) {
    return LogLevel2String(level);
}

template<>
inline String LexicalCast<String, LogLevel>(const LogLevel& level) {
    return LogLevel2String(level);
}

template<>
inline LogLevel LexicalCast<LogLevel, StringArg>(const StringArg& str) {
    return String2LogLevel(str);
}

template<>
inline LogLevel LexicalCast<LogLevel, String>(const String& str) {
    return String2LogLevel(str);
}

} //namespace nemo