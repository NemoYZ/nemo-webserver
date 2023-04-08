#pragma once

#include <stdint.h>

#include <memory>
#include <mutex>

#include "common/types.h"
#include "log/log_level.h"
#include "util/timestamp.h"

namespace nemo {

class Logger;

/**
 * 日志事件
 */
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<LogEvent> UniquePtr; ///< 智能指针定义
    /**
     * @brief LogEvent的构造函数
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] filename 文件名
     * @param[in] line 文件行号
     * @param[in] elapse 程序启动依赖的耗时(毫秒)
     * @param[in] threadId 线程id
     * @param[in] taskId 协程id
     * @param[in] time 日志事件(秒)
     * @param[in] threadName 线程名称
     */
    LogEvent(std::shared_ptr<Logger> logger, LogLevel level,
            StringArg filename, int32_t line, uint32_t elapse,
            uint32_t threadId, uint32_t taskId, Timestamp time,
            StringArg threadName);
    /**
     * @brief 返回文件名
     */
    const String& getFileName() const { return filename_; }

    /**
     * @brief 返回行号
     */
    int32_t getLine() const { return line_; }

    /**
     * @biref 返回耗时
     */
    uint32_t getElapse() const { return elapse_; }

    /**
     * @brief 返回线程ID
     */
    uint32_t getThreadId() const { return threadId_; }

    /**
     * @brief 返回协程ID
     */
    uint32_t getTaskId() const { return taskId_; }

    /**
     * @brief 返回时间
     */
    Timestamp getTimestamp() const { return timestamp_; }

    /**
     * @brief 返回线程名称
     */
    const String& getThreadName() const { return threadName_; }

    /**
     * @brief 返回日志内容
     */
    String getContent() const { return ss_.str(); }

    /**
     * @brief 返回日志器
     */
    std::shared_ptr<Logger> getLogger() const { return logger_; }

    /**
     * @brief 返回日志级别
     */
    LogLevel getLevel() const { return level_; }

    /**
     * @brief 返回日志内容字符串流
     */
    std::stringstream& getSS() { return ss_; }

    /**
     * @brief 格式化写入日志内容
     */
    void format(const char* fmt, ...);

    /**
     * @brief 格式化写入日志内容
     */
    void format(const char* fmt, va_list al);

private:
    String filename_;                ///< 文件路径
    int32_t line_ = 0;               ///< 行号
    uint32_t elapse_ = 0;            ///< 程序启动开始到现在的毫秒数
    uint32_t threadId_ = 0;         ///< 线程ID
    uint32_t taskId_ = 0;           ///< 协程ID
    Timestamp timestamp_;           ///< 时间戳
    String threadName_;             ///< 线程名称
    std::stringstream ss_;           ///< 日志流内容
    std::shared_ptr<Logger> logger_; ///< 日志器
    LogLevel level_;                 ///< 日志等级
};

/**
 * @brief 日志事件包装器
 */
class LogEventWrap {
public:
    /**
     * @brief 构造函数
     * @param[in] event 日志事件
     */
    LogEventWrap(std::shared_ptr<LogEvent> event);

    /**
     * @brief 析构函数
     */
    ~LogEventWrap();

    /**
     * @brief 获取日志事件
     */
    LogEvent* getEvent() const { return event_.get(); }

    /**
     * @brief 获取日志内容流
     */
    std::stringstream& getSS() const { return event_->getSS(); }

private:
    std::shared_ptr<LogEvent> event_;  ///< 日志事件
};

}