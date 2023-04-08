#pragma once

#include "common/thread.h"
#include "common/singleton.h"
#include "util/timestamp.h"
#include "util/util.h"
#include "log/logger.h"
#include "log/log_event.h"

/**
 * @brief 使用流的方式将级别为level的日志写入到logger
 * __FILE__宏可以输出源文件的路径，__LINE__宏可以输出使用__LINE__宏的行数
 */
#define NEMO_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        nemo::LogEventWrap(std::make_shared<nemo::LogEvent>(logger, level, \
                __FILE__, __LINE__, 0, nemo::GetCurrentThreadId(),\
                nemo::GetCurrentTaskId(), nemo::Timestamp::Now(),  \
                nemo::Thread::GetCurrentThreadName())).getSS()

/**
 * @brief 使用流方式将级别为debug的日志写入到logger
 */
#define NEMO_LOG_DEBUG(logger) NEMO_LOG_LEVEL(logger, nemo::LogLevel::DEBUG)

/**
 * @brief 使用流方式将级别为info的日志写入到logger
 */
#define NEMO_LOG_INFO(logger) NEMO_LOG_LEVEL(logger, nemo::LogLevel::INFO)

/**
 * @brief 使用流方式将级别为warn的日志写入到logger
 */
#define NEMO_LOG_WARN(logger) NEMO_LOG_LEVEL(logger, nemo::LogLevel::WARN)

/**
 * @brief 使用流方式将级别为error的日志写入到logger
 */
#define NEMO_LOG_ERROR(logger) NEMO_LOG_LEVEL(logger, nemo::LogLevel::ERROR)

/**
 * @brief 使用流方式将级别为fatal的日志写入到logger
 */
#define NEMO_LOG_FATAL(logger) NEMO_LOG_LEVEL(logger, nemo::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志写入到logger
 * __VA_ARGS__宏是可变参数的宏(三个点)
 * __VA_ARGS__的内容替换第一行fmt中三个点中的内容
 */
#define NEMO_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        nemo::LogEventWrap(std::make_unique<nemo::LogEvent>(logger, level, \
                           __FILE__, __LINE__, 0, nemo::GetCurrentThreadId(),\
                           nemo::GetCurrentTaskId(), time(0),  \
                           nemo::Thread::GetCurrentThreadName())).           \
                           getEvent()->format(fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别debug的日志写入到logger
 */
#define NEMO_LOG_FMT_DEBUG(logger, fmt, ...) NEMO_LOG_FMT_LEVEL(logger, nemo::LogLevel::DEBUG, fmt, __VA_ARGS__)

/**
 * @brief 使用流方式将级别为info的日志写入到logger
 */
#define NEMO_LOG_FMT_INFO(logger, fmt, ...)  NEMO_LOG_FMT_LEVEL(logger, nemo::LogLevel::INFO, fmt, __VA_ARGS__)

/**
 * @brief 使用流方式将级别为warn的日志写入到logger
 */
#define NEMO_LOG_FMT_WARN(logger, fmt, ...)  NEMO_LOG_FMT_LEVEL(logger, nemo::LogLevel::WARN, fmt, __VA_ARGS__)
/**
 * @brief 使用流方式将级别为error的日志写入到logger
 */
#define NEMO_LOG_FMT_ERROR(logger, fmt, ...) NEMO_LOG_FMT_LEVEL(logger, nemo::LogLevel::ERROR, fmt, __VA_ARGS__)

/**
 * @brief 使用流方式将级别为fatal的日志写入到logger
 */
#define NEMO_LOG_FMT_FATAL(logger, fmt, ...) NEMO_LOG_FMT_LEVEL(logger, nemo::LogLevel::FATAL, fmt, __VA_ARGS__)

/**
 * @brief 获取root日志器
 */
#define NEMO_LOG_ROOT() nemo::LoggerManager::GetInstance().getRootLogger()

/**
 * @brief 直接用root用户打日志
 */
#define NEMO_ROOT_DEBUG() NEMO_LOG_DEBUG(NEMO_LOG_ROOT())
#define NEMO_ROOT_INFO() NEMO_LOG_INFO(NEMO_LOG_ROOT())
#define NEMO_ROOT_WARN() NEMO_LOG_WARN(NEMO_LOG_ROOT())
#define NEMO_ROOT_ERROR() NEMO_LOG_ERROR(NEMO_LOG_ROOT())

/**
 * @brief 获取name的日志器
 */
#define NEMO_LOG_NAME(name) nemo::LoggerManager::GetInstance().getLogger(name)

namespace nemo {

}   //namespace nemo