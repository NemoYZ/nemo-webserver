#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>

#include "common/thread.h"
#include "log/log_level.h"
#include "container/buffer.h"
#include "util/file_appender.h"

namespace nemo {

class Logger;
class LogEvent;
class LogFormatter;

/**
 * @brief 日志输出目标
 */
class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<LogAppender> UniquePtr; ///< 智能指针定义
    
public:
    enum LogAppenderType {
        STDOUT_LOG_APPENDER,
        FILE_LOG_APPENDER,
        UNKNOWN
    };

public:
    LogAppender() = default;
    /**
     * @brief 析构函数
     */
    virtual ~LogAppender() = default;
    
    /**
     * @brief 写入日志
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] event 日志事件
     */
    virtual void log(std::shared_ptr<Logger> logger,
            LogLevel level,
            std::shared_ptr<LogEvent> event) = 0;

    /**
     * @brief 将日志输出目标的配置转成YAML String
     */
    virtual String toYamlString() = 0;

    /**
     * @brief 刷新
     */
    virtual void flush() = 0;

    /**
     * @brief 设置日志格式器
     */
    void setFormatter(std::shared_ptr<LogFormatter> formatter);

    /**
     * @brief 获取日志格式器
     */
    LogFormatter* getFormatter() { return formatter_.get(); }

    /**
     * @brief 获取日志级别
     */
    LogLevel getLevel() const { return level_; }

    virtual LogAppenderType getType() const { return LogAppenderType::UNKNOWN; }

    /**
     * @brief 设置日志级别
     * @param[in] level 级别
     */
    void setLevel(LogLevel level) { level_ = level; }

protected:
    LogLevel level_ = LogLevel::DEBUG;        ///< 日志级别, 默认DEBUG
    std::shared_ptr<LogFormatter> formatter_; ///< 日志格式器
    std::mutex mutex_;                         
    bool hasFormatter_ = false;               ///< 是否有自己的日志格式器
};

/**
 * @brief 输出到控制台的Appender
 */
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<StdoutLogAppender> UniquePtr; ///< 智能指针定义

public:
    StdoutLogAppender() = default;
    virtual ~StdoutLogAppender() = default;

public:
    virtual void flush() override;
    virtual String toYamlString() override;
    virtual void log(std::shared_ptr<Logger> logger,
            LogLevel level,
            std::shared_ptr<LogEvent> event) override;
    virtual LogAppenderType getType() const override { return LogAppenderType::STDOUT_LOG_APPENDER; }
};

/**
 * @brief 输出到文件的Appender
 */
class FileLogAppender : public LogAppender {
public:
    constexpr static uint32_t kRollThreshold = 1000 * 1000 * 1000;  ///< 默认回滚大小1GB
    constexpr static uint32_t kCheckThreshold = 1024;               ///< 默认写日志次数后刷新
    constexpr static uint32_t kRollPerSeconds = 60 * 60 * 24; ///< 一天
    constexpr static uint32_t kFlushInterval = 3;   ///< 默认缓冲区刷新时间3s

public:
    typedef std::shared_ptr<FileLogAppender> ptr; ///< 智能指针定义
    FileLogAppender(const std::string_view& filename, size_t flushInterval = kFlushInterval,
            off_t rollThreshold = kRollThreshold, uint32_t checkThreshold = kCheckThreshold);
    virtual ~FileLogAppender();
    String toYamlString() override;
    virtual void log(std::shared_ptr<Logger> logger,
            LogLevel level,
            std::shared_ptr<LogEvent> event) override;
    virtual LogAppenderType getType() const override { 
        return LogAppenderType::FILE_LOG_APPENDER; 
    }
    virtual void flush() override;
    void start();
    void stop();
    bool rollFile();
    
protected:
    typedef Buffer::UniquePtr BufferPtr;
    typedef std::vector<BufferPtr> BufferVector;
private:
    void consumeFunc();

private:
    static String GetLogFileName(const String& filename, time_t* now);
    void write(const BufferVector& buffVec);

private:
    String filename_;
    size_t flushInterval_;                            ///< 刷新间隔
    off_t rollThreshold_;                             ///< 回滚门槛
    time_t startOfPeriod_;                           ///< 本文件开始打日志的时间 
    time_t lastRoll_;                                 ///< 上一次回滚
    time_t lastFlush_;                                ///< 上一次刷新
    Thread::UniquePtr thread_;                         ///< 消费者线程
    BufferPtr currBuff_;                              ///< 当前缓冲
    BufferPtr nextBuff_;                              ///< 下一个缓冲
    BufferVector buffers_;                             ///< 多缓冲
    file_util::FileAppender::UniquePtr fileAppender_; ///< 输出文件
    uint32_t checkThreshold_;                         ///< 每隔多久检查一次
    uint32_t writeCount_;                             ///< 写次数
    std::condition_variable cond_;                    ///< 条件变量
    std::atomic<bool> running_;                        ///< 是否运行消费者线程
};

} //namespace nemo