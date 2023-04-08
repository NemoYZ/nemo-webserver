#include "log/log.h"

#include <string.h>
#include <stdio.h>
#include <sys/param.h>

#include <typeinfo>
#include <iostream>

#include "common/macro.h"
#include "common/config.h"
#include "common/lexical_cast.h"
#include "log/log_appender.h"
#include "log/log_formatter.h"

namespace nemo {

String LoggerManager::toYamlString() {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    YAML::Node node;
    for(auto& logger : loggers_) {
        node.push_back(YAML::Load(logger.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

struct LogAppenderDefine {
    int type = 0; //1 File, 2 Stdout
    LogLevel level = LogLevel::ALL;
    String format;
    String file;
    size_t flushInterval = FileLogAppender::kFlushInterval;     //for FileLogAppender
    off_t rollThreshold = FileLogAppender::kRollThreshold;      //for FileLogAppender
    size_t checkThreshold = FileLogAppender::kCheckThreshold;   //for FileLogAppender

    bool operator==(const LogAppenderDefine& other) const {
        return type == other.type && 
            level == other.level && 
            format == other.format && 
            file == other.file && 
            flushInterval == other.flushInterval;
    }
};

struct LogDefine {
    LogLevel level = LogLevel::ALL;
    std::vector<LogAppenderDefine> appenders;
    String name;
    String format;

    bool operator==(const LogDefine& other) const {
        return name == other.name && 
               level == other.level && 
               appenders == other.appenders && 
               format == other.format;
    }

    bool operator!=(const LogDefine& other) const {
        return !(*this == other);
    }

    bool operator<(const LogDefine& other) const {
        return name < other.name;
    }
};


template<>
LogDefine LexicalCast<LogDefine, String>(const String& filename) {
    YAML::Node node = YAML::Load(filename);
    LogDefine logDefine;
    if(!node["name"].IsDefined()) {
        std::cout << "log config error: name is null, " 
                    << node
                    << std::endl;
        throw std::logic_error("log config name is null");
    }
    logDefine.name = node["name"].as<String>();
    logDefine.level = String2LogLevel(node["level"].IsDefined() ? node["level"].as<String>() : "");
    if(node["format"].IsDefined()) {
        logDefine.format = node["format"].as<String>();
    }

    if(node["appenders"].IsDefined()) {
        for(size_t i = 0; i < node["appenders"].size(); ++i) {
            auto appender = node["appenders"][i];
            if(!appender["type"].IsDefined()) {
                std::cout << "log config error: appender type is null, " 
                            << appender
                            << std::endl;
                continue;
            }
            String type = appender["type"].as<String>();
            LogAppenderDefine logAppenderDefine;
            if(type == "FileLogAppender") {
                logAppenderDefine.type = 1;
                if(!appender["file"].IsDefined()) {
                    std::cout << "log config error: fileappender file is null, " 
                                << appender
                                << std::endl;
                    continue;
                }
                logAppenderDefine.file = appender["file"].as<String>();
                if(appender["format"].IsDefined()) {
                    logAppenderDefine.format = appender["format"].as<String>();
                }
                if (appender["flush_interval"].IsDefined()) {
                    logAppenderDefine.flushInterval = appender["flush_interval"].as<size_t>();    
                }
                if (appender["roll_threshold"].IsDefined()) {
                    logAppenderDefine.rollThreshold = appender["roll_threshold"].as<off_t>();
                }
                if (appender["check_threshold"].IsDefined()) {
                    logAppenderDefine.checkThreshold = appender["check_threshold"].as<size_t>();
                }
            } else if(type == "StdoutLogAppender") {
                //程序启动文件先于日志配置文件加载
                //守护进程不需要标准输出
                /*
                if (Application::IsDaemon()) { 
                    continue;
                }
                */
                logAppenderDefine.type = 2;
                if(appender["format"].IsDefined()) {
                    logAppenderDefine.format = appender["format"].as<String>();
                }
                if (appender["flush_interval"].IsDefined()) {
                    logAppenderDefine.flushInterval = appender["flush_interval"].as<size_t>();
                }
            } else {
                std::cout << "log config error: appender type is invalid, " 
                            << appender
                            << std::endl;
                continue;
            }

            logDefine.appenders.push_back(logAppenderDefine);
        }
    }
    return logDefine;
}

template<>
String LexicalCast<String, LogDefine>(const LogDefine& logDefine) {
    YAML::Node node;
    node["name"] = logDefine.name;
    node["level"] = LogLevel2String(logDefine.level);
    if(!logDefine.format.empty()) {
        node["format"] = logDefine.format;
    }

    for(auto& appender : logDefine.appenders) {
        YAML::Node appenderNode;
        if(1 == appender.type) {
            appenderNode["type"] = "FileLogAppender";
            appenderNode["file"] = appender.file;
            appenderNode["flush_interval"] = appender.flushInterval;
            appenderNode["roll_threshold"] = appender.rollThreshold;
            appenderNode["check_threshold"] = appender.checkThreshold;
        } else if(2 == appender.type) {
            appenderNode["type"] = "StdoutLogAppender";
            appenderNode["flush_interval"] = appender.flushInterval;
        }
        appenderNode["level"] = LogLevel2String(appender.level);
        
        if(!appender.format.empty()) {
            appenderNode["format"] = appender.format;
        }

        node["appenders"].push_back(appenderNode);
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

//想了一下，为什么不用unordered_set而是用set
//首先，需要自己实现一个hash
//hash的设计是个问题
//其次，如果使用万能的hash_val，会导致计算hash值的时间增长
//所以还不如用set
//全局变量会在进入main函数之前初始化
static nemo::ConfigVar<std::set<LogDefine>>* gLogDefines =
    nemo::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
    LogIniter() {
        gLogDefines->addListener([](const std::set<LogDefine>& oldValue, const std::set<LogDefine>& newValue) {
            NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "on_logger_conf_changed";
            for(auto& logDefine : newValue) {
                auto iter = oldValue.find(logDefine);
                nemo::Logger::SharedPtr logger;
                if(iter == oldValue.end()) {
                    //新增logger
                    logger = NEMO_LOG_NAME(logDefine.name);
                } else {
                    if(logDefine != *iter) {
                        //修改的logger
                        logger = NEMO_LOG_NAME(logDefine.name);
                    }
                }
                logger->setLevel(logDefine.level);
                if(!logDefine.format.empty()) {
                    logger->setFormat(logDefine.format);
                }

                logger->clearAppenders();
                for(auto& appender : logDefine.appenders) {
                    nemo::LogAppender::SharedPtr newAppender;
                    if(1 == appender.type) {
                        newAppender.reset(new FileLogAppender(appender.file, 
                            appender.flushInterval, appender.rollThreshold,
                            appender.checkThreshold));
                    } else if(2 == appender.type) {
                        newAppender.reset(new StdoutLogAppender);
                    }
                    newAppender->setLevel(appender.level);
                    if (!appender.format.empty()) {
                        LogFormatter::SharedPtr fmt = std::make_shared<LogFormatter>(appender.format);
                        if (!fmt->hasError()){
                            newAppender->setFormatter(fmt);
                        } else {
                            std::cout << "log.name=" 
                                      << logDefine.name 
                                      << " appender type=" 
                                      << appender.type
                                      << " is invalid" 
                                      << std::endl;
                        }
                    }
                    logger->addAppender(newAppender);
                }
            }
            
            for(auto& logDefine : oldValue) {
                auto iter = newValue.find(logDefine);
                if(iter == newValue.end()) {
                    std::cout << "删除logger" << std::endl;
                    //删除logger
                    auto logger = NEMO_LOG_NAME(logDefine.name);
                    logger->setLevel((LogLevel)0);
                    logger->clearAppenders();
                }
            }
        });
    }
};

static LogIniter gLogIniter; //全局变量的构造函数会在进入main函数之前被调用

}   //namespace nemo