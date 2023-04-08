#pragma once

#include <memory>
#include <vector>

#include "common/types.h"
#include "log/log_level.h"

namespace nemo {

class Logger;
class LogEvent;

/**
 * @brief 日志格式化
 */
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<LogFormatter> UniquePtr; ///< 智能指针定义
    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板
     * @details
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     *
     *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
     */
    LogFormatter(StringArg pattern);

    /**
     * @brief 返回格式化日志文本
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] event 日志事件
     */
    String format(std::shared_ptr<Logger> logger,
                LogLevel level,
                std::shared_ptr<LogEvent> event);

    std::ostream& format(std::ostream& os,
                         std::shared_ptr<Logger> logger,
                         LogLevel level,
                         std::shared_ptr<LogEvent> event);
public:
    /**
     * @brief 日志内容项格式化
     */
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> SharedPtr; ///< 智能指针定义
        typedef std::unique_ptr<FormatItem> UniquePtr; ///< 智能指针定义
    public:
        enum FormatItemType {
            MESSAGE     = 0,
            LOG_LEVEL   = 1,
            ELAPSE      = 2,
            LOG_NAME    = 3,
            THREAD_ID   = 4,
            TASK_ID     = 5,
            THREAD_NAME = 6,
            DATE        = 7,
            FILENAME    = 8,
            LINE        = 9,
            NEW_LINE    = 10,
            STRING      = 11,
            TAB         = 12,
        };

    public:
        /**
         * @brief 析构函数
         */
        virtual ~FormatItem() = default;

        /**
         * @brief 格式化日志到流
         * @param[in, out] os 日志输出流
         * @param[in] logger 日志器
         * @param[in] level 日志等级
         * @param[in] event 日志事件
         */
        virtual void format(std::ostream& os,
                             std::shared_ptr<Logger> logger,
                             LogLevel level,
                             std::shared_ptr<LogEvent> event) = 0;
        virtual FormatItemType getType() const = 0;
    };
    
    /**
     * @brief 初始化,解析日志模板
     */
    void parse();

    /**
     * @brief 是否有错误
     */
    bool hasError() const { return hasError_; }

    /**
     * @brief 返回日志模板
     */
    const String& getPattern() const { return pattern_; }

#ifdef LOG_DEBUG
public:
    std::vector<FormatItem::UniquePtr>& getItems() {
        return items_;
    }
#endif //LOG_DEBUG

private:
    bool hasError_ = false;                   ///< 是否有错
    std::vector<FormatItem::UniquePtr> items_; ///< 日志格式解析后的格式
    String pattern_;                            ///< 日志格式模板
};

} //namespace nemo