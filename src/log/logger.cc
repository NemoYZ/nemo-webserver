#include "log/logger.h"

#include <iostream>

#include <yaml-cpp/yaml.h>

#include "log/log_formatter.h"
#include "log/log_appender.h"

namespace nemo {

Logger::Logger(StringArg name) :
    name_(name.data(), name.size()),
    level_(LogLevel::DEBUG) {
    formatter_= std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");
}

Logger::~Logger() {
}

void Logger::log(LogLevel level, std::shared_ptr<LogEvent> event) {
    if(level >= level_) {
        auto pself = shared_from_this(); //保证要在打日志的时候本logger不会被释放
        std::lock_guard<std::mutex> lockGuard(mutex_);
        if (!appenders_.empty()) {
            for(auto& appender : appenders_) {
                appender->log(pself, level, event);
            }
        } else if (rootLogger_) {
            rootLogger_->log(level, event);
        }
    }
}

void Logger::addAppender(std::shared_ptr<LogAppender> appender) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    if(!appender->getFormatter()) {
        appender->setFormatter(formatter_);
    }
    appenders_.push_back(appender);
}

bool Logger::delAppender(std::shared_ptr<LogAppender> appender) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    auto iter = std::find(appenders_.begin(), appenders_.end(), appender);
    if (iter != appenders_.end()) {
        appenders_.erase(iter);
        return true;
    }

    return false;
}

void Logger::clearAppenders() {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    appenders_.clear();
}

void Logger::setFormatter(std::shared_ptr<LogFormatter> formatter) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    formatter_ = formatter;

    for(auto& appender : appenders_) {
        std::lock_guard<std::mutex> lockGuard(appender->mutex_);
        if(!appender->hasFormatter_) {
            appender->setFormatter(formatter);
        }
    }
}

void Logger::setFormat(StringArg format) {
    LogFormatter::SharedPtr newLogFormatter = std::make_shared<LogFormatter>(format);
    if(newLogFormatter->hasError()) {
        std::cout << "Logger setFormatter name=" 
                  << name_
                  << " format=" 
                  << format 
                  << " invalid formatter"
                  << std::endl;
        return;
    }
    setFormatter(newLogFormatter);  //交给参数为智能指针的来处理
}

String Logger::toYamlString() {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    YAML::Node node;
    node["name"] = name_;
    node["level"] = LexicalCast<String>(level_);
    if(formatter_) {
        node["formatter"] = formatter_->getPattern();
    }

    for(auto& appender : appenders_) {
        node["appenders"].push_back(YAML::Load(appender->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LoggerManager::LoggerManager(Token token) :
    rootLogger_(std::make_shared<Logger>()) {
    rootLogger_->addAppender(LogAppender::SharedPtr(new StdoutLogAppender));
    loggers_[rootLogger_->name_]= rootLogger_;
}

Logger::SharedPtr LoggerManager::getLogger(const String& name) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    auto iter = loggers_.find(name);
    if (iter != loggers_.end()) {
        return iter->second;
    }

    Logger::SharedPtr logger = std::make_shared<Logger>(name);
    logger->rootLogger_ = rootLogger_;
    loggers_[name] = logger;
    return logger;
}

} //namespace nemo