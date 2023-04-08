#pragma once

#include <memory>
#include <vector>
#include <list>
#include <mutex>

#include "common/types.h"
#include "common/singleton.h"
#include "log/log_level.h"

namespace nemo {

class LogEvent;
class LogAppender;
class LogFormatter;

/**
 * @brief 日志器
 */
class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> SharedPtr;  ///< 智能指针定义
    typedef std::unique_ptr<Logger> UniquePtr;  ///< 智能指针定义
    
    /**
     * @brief 构造函数
     * @param[in] name 日志器名称，默认为root
     */
    Logger(StringArg name = "root");

    /**
     * @brief 析构
     */
    ~Logger();

    /**
     * @brief 写日志
     * @param[in] level 日志级别
     * @param[in] event 日志事件
     */
    void log(LogLevel level, std::shared_ptr<LogEvent> event);

    /**
     * @brief 写ALL级别日志
     * @param[in] event 日志事件
     */
    void all(std::shared_ptr<LogEvent> event) { 
        log(LogLevel::ALL, event); 
    }
    
    /**
     * @brief 写debug级别日志
     * @param[in] event 日志事件
     */
    void debug(std::shared_ptr<LogEvent> event) { 
        log(LogLevel::DEBUG, event); 
    }

    /**
     * @brief 写info级别日志
     * @param[in] event 日志事件
     */
    void info(std::shared_ptr<LogEvent> event) {
        log(LogLevel::INFO, event); 
    }

    /**
     * @brief 写warn级别日志
     * @param[in] event 日志事件
     */
    void warn(std::shared_ptr<LogEvent> event) { 
        log(LogLevel::WARN, event); 
    }

    /**
     * @brief 写error级别日志
     * @param[in] event 日志事件
     */
    void error(std::shared_ptr<LogEvent> event) { 
        log(LogLevel::ERROR, event); 
    }

    /**
     * @brief 写fatal级别日志
     * @param[in] event 日志事件
     */
    void fatal(std::shared_ptr<LogEvent> event) { 
        log(LogLevel::FATAL, event); 
        std::abort();
    }
    
    /**
     * @brief 添加日志目标
     * @param[in] appender 日志目标
     */
    void addAppender(std::shared_ptr<LogAppender> appender);

    /**
     * @brief 删除日志目标
     * @param[in] appender 日志目标
     */
    bool delAppender(std::shared_ptr<LogAppender> appender);

    /**
     * @brief 清空日志目标
     */
    void clearAppenders();

    /**
     * @brief 返回日志级别
     */
    LogLevel getLevel() const { return level_; }

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel level) { level_ = level; }
    
    /**
     * @brief 返回日志名称
     */
    const String& getName() const { return name_; }
    
    /**
     * @brief 设置日志格式器
     */
    void setFormatter(std::shared_ptr<LogFormatter> formatter);

    /**
     * @brief 设置日志格式模板
     */
    void setFormat(StringArg format);

    /**
     * @brief 获取日志格式器
     */
    LogFormatter* getFormatter() { return formatter_.get(); }
    
    /**
     * @brief 将日志器的配置转成YAML String
     */
    String toYamlString();

private:
    typedef std::list<std::shared_ptr<LogAppender>> AppenderVec;

private:
    String name_;                               ///< 日志名称
    LogLevel level_;                            ///< 日志级别
    std::mutex mutex_;
    AppenderVec appenders_;                     ///< 日志目标集合
    std::shared_ptr<LogFormatter> formatter_;   ///< 日志格式器
    Logger::SharedPtr rootLogger_;              ///< 主日志器
};

/**
 * @brief 日志器管理类
 */
class LoggerManager : public Singleton<LoggerManager> {
public:
    /**
     * @brief 构造函数
     */
    LoggerManager(Token token);

    /**
     * @brief 获取日志器
     * @param[in] name 日志器名称
     */
    Logger::SharedPtr getLogger(const String& name);

    /**
     * @brief 返回root日志器
     */
    Logger::SharedPtr getRootLogger() { return rootLogger_; }

    /**
     * @brief 将所有的日志器配置转成YAML String
     */
    String toYamlString();

    void foreachLogger(std::function<void(Logger::SharedPtr logger)> f);

private:
    typedef std::unordered_map<String, Logger::SharedPtr> LoggerMap;

private:
    std::mutex mutex_;
    Logger::SharedPtr rootLogger_;  ///< root日志器
    LoggerMap loggers_;             ///< 日志器容器
};

} //namespace nemo