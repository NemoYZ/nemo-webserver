#define LOG_DEBUG

#include <iostream>
#include <vector>
#include <thread>
#include <yaml-cpp/yaml.h>
#include "log/log.h"
#include "log/log_appender.h"
#include "log/log_event.h"
#include "log/log_formatter.h"
#include "util/util.h"
#include "common/config.h"
#include "common/types.h"

using namespace nemo;

void LogLevelTest();
void LogFormatterTest();
void LogEventTest();
void LogAppenderTest();
void LoggerTest();
void LogFunc();
void Benchmark();

int main(int argc, char* argv[]) {
    //LogLevelTest();
    //LogFormatterTest();
    //LogEventTest();
    //LogAppenderTest();
    //LoggerTest();
    Benchmark();
    
    return 0;
}

void LogLevelTest() {
    LogLevel level = LogLevel::DEBUG;
    String levelStr = LexicalCast<String>(level);
    LogLevel level2 = LexicalCast<LogLevel>(levelStr);
    std::cout << "levelStr=" << levelStr << std::endl;
    std::cout << "level2=" << LexicalCast<String>(level2) << std::endl;
}

void LogFormatterTest() {
    LogFormatter::SharedPtr formatter = std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");
    std::cout << formatter->getPattern() << std::endl;
    std::vector<LogFormatter::FormatItem::UniquePtr>& items = formatter->getItems();
    for (auto& item : items) {
        std::cout << item->getType() << std::endl;
    }
}

void LogEventTest() {
    Logger::SharedPtr logger = std::make_shared<Logger>("log_event_test_logger");
    LogEvent::SharedPtr event = std::make_shared<LogEvent>(logger, LogLevel::DEBUG,
                __FILE__, __LINE__, 0, nemo::GetCurrentThreadId(),
                nemo::GetCurrentTaskId(), ::time(nullptr),
                nemo::Thread::GetCurrentThreadName());
    event->getSS() << "change the boss of this gym" << std::endl;
    std::cout << event->getSS().str() << std::endl;
}

void LogAppenderTest() {
    LogAppender::SharedPtr appender1(new StdoutLogAppender);
    Logger::SharedPtr logger = std::make_shared<Logger>("log_event_test_logger");
    LogEvent::SharedPtr event = std::make_shared<LogEvent>(logger, LogLevel::DEBUG,
                __FILE__, __LINE__, 0, nemo::GetCurrentThreadId(),
                nemo::GetCurrentTaskId(), ::time(nullptr),
                nemo::Thread::GetCurrentThreadName());
    LogFormatter::SharedPtr formatter = std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");
    appender1->setFormatter(formatter);
    std::cout << appender1->getFormatter() << std::endl;
    appender1->log(logger, event->getLevel(), event);
}

void LoggerTest() {
    Logger::SharedPtr logger = NEMO_LOG_NAME("system");
    
    NEMO_LOG_DEBUG(logger) << "change the boss of this gym";
}

static Logger::SharedPtr gRootLogger = NEMO_LOG_NAME("root");
static Logger::SharedPtr gSystemLogger = NEMO_LOG_NAME("system");

void LogFunc() {
    for (int i = 0; i < 1000000; ++i) {
        NEMO_LOG_DEBUG(gSystemLogger) << LexicalCast<String>(i);
    }
}

void Benchmark() {
    String configPath = "/programs/nemo/bin/config";
    Config::LoadFromDir(configPath);
    std::jthread logThread(LogFunc);
}