#include "log/log_event.h"

#include <stdarg.h>

#include "log/logger.h"

namespace nemo {

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel level,
            StringArg filename, int32_t line, uint32_t elapse,
            uint32_t threadId, uint32_t fiberId, Timestamp time,
            StringArg threadName) :
    filename_(filename.data(), filename.size()),
    line_(line),
    elapse_(elapse),
    threadId_(threadId),
    taskId_(fiberId),
    timestamp_(time),
    threadName_(threadName.data(), threadName.size()),
    logger_(logger),
    level_(level) {
}

void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
    char* buf = nullptr;
    int len = ::vasprintf(&buf, fmt, al); //vasprint和vsprint类似，只不过vasprint会申请一片足够大的空间来存放解析结果
    if(len != -1) {
        ss_ << String(buf, len);
        ::free(buf);
    }
}
 
LogEventWrap::LogEventWrap(std::shared_ptr<LogEvent> event) : 
    event_(event) {
}

LogEventWrap::~LogEventWrap() {
    event_->getLogger()->log(event_->getLevel(), event_); //在析构的时候打印日志
}

} //namespace nemo