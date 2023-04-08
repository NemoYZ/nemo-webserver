#include "log/log_appender.h"

#include <iostream>

#include "common/config.h"
#include "common/macro.h"
#include "common/lexical_cast.h"
#include "system/env.h"
//#include "application.h"
#include "log/log_formatter.h"
#include "util/timestamp.h"

namespace nemo {

static ConfigVar<uint32_t>* gLogAppenderFlushInterval = 
    Config::Lookup<uint32_t>("file_log_appender.flush_interval", 
                             FileLogAppender::kFlushInterval, 
                             "file log appender flush_interval");

static ConfigVar<uint32_t>* gFileRollSize = 
    Config::Lookup<uint32_t>("file_appender.roll_threshold", 
                             FileLogAppender::kRollThreshold, 
                             "file appender roll threshold");

static ConfigVar<uint32_t>* gFileCheckEveryN = 
    Config::Lookup<uint32_t>("file_appender.check_threshold", 
                             FileLogAppender::kCheckThreshold, 
                             "file appender check threshold");

void LogAppender::setFormatter(std::shared_ptr<LogFormatter> formatter) {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    formatter_ = formatter;
    if (formatter_) {
        hasFormatter_ = true;
    } else {
        hasFormatter_ = false;
    }
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger,
                            LogLevel level,
                            std::shared_ptr<LogEvent> event) {
    if(level >= level_) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        std::cout << formatter_->format(logger, level, event);
    }
}

void StdoutLogAppender::flush() {
    fflush(stdout);
}

String StdoutLogAppender::toYamlString() {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if(hasFormatter_ && formatter_) {
        node["formatter"] = formatter_->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

String FileLogAppender::GetLogFileName(const String& filename, time_t* now) {
    String::size_type slashPos = filename.find_last_of('/');
    NEMO_ASSERT(slashPos != String::npos);
    String newFilename(filename.c_str(), slashPos + 1);    //得到目录路径
    newFilename.reserve(filename.size());

    char timeBuf[32];
    struct tm tm;
    *now = time(nullptr);
    ::gmtime_r(now, &tm); // FIXME: localtime_r ?
    size_t n = strftime(timeBuf, sizeof(timeBuf), ".%Y%m%d-%H%M%S", &tm);
    newFilename.append(timeBuf, n);

    newFilename += Env::GetInstance().getHostName();
    newFilename += LexicalCast<String>(Env::GetInstance().getPid());
    newFilename.append(".log", 4);

    return newFilename;
}

FileLogAppender::FileLogAppender(const std::string_view& filename, size_t flushInterval,
        off_t rollThreshold, uint32_t checkThreshold) :
    filename_(filename),
    flushInterval_(flushInterval),
    rollThreshold_(rollThreshold),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0),
    currBuff_(new Buffer(EXEC_PAGESIZE, true)),
    nextBuff_(new Buffer(EXEC_PAGESIZE, true)),
    fileAppender_(new file_util::FileAppender(filename)),
    checkThreshold_(checkThreshold),
    writeCount_(0),
    running_(false) {
    currBuff_->bzero();
    nextBuff_->bzero();
    rollFile();
}

FileLogAppender::~FileLogAppender() {
    stop();
    //防止程序意外退出而丢失日志
    if (currBuff_ && !currBuff_->empty()) {
        buffers_.emplace_back(std::move(currBuff_));
    }
    if (nextBuff_ && !nextBuff_->empty()) {
        buffers_.emplace_back(std::move(nextBuff_));
    }
    write(buffers_);
}

void FileLogAppender::log(std::shared_ptr<Logger> logger,
                    LogLevel level,
                    std::shared_ptr<LogEvent> event) {
    if (NEMO_UNLIKELY(!running_)) {
        start();
    }
    if(level >= level_) {
        std::unique_lock<std::mutex> uniqueLock(mutex_);
        String str = formatter_->format(logger, level, event);
        if (currBuff_->avail() > str.size()) {
            currBuff_->append(str.data(), str.size());
        } else {
            buffers_.emplace_back(std::move(currBuff_));
            if (nextBuff_) {
                currBuff_ = std::move(nextBuff_);
            } else {
                currBuff_.reset(new Buffer(EXEC_PAGESIZE, true));
            }
            currBuff_->append(str.data(), str.size());
            cond_.notify_one();
        }
    }
}

void FileLogAppender::write(const BufferVector& buffVec) {
    for (const BufferPtr& buff : buffVec) {
        fileAppender_->append(buff->data(), buff->size());
        if (fileAppender_->getWrittenBytes() > rollThreshold_) {
            rollFile();
        } else {
            ++writeCount_;
            if (writeCount_ >= checkThreshold_) {
                writeCount_ = 0;
                time_t now = ::time(nullptr);
                time_t thisPeriod = now / kRollPerSeconds * kRollPerSeconds;
                if (thisPeriod != startOfPeriod_) {
                    rollFile();
                } else if (static_cast<size_t>(now - lastFlush_) > flushInterval_) {
                    lastFlush_ = now;
                    fileAppender_->flush();
                }
            }
        }
    }
}

void FileLogAppender::flush() {
    fileAppender_->flush();
}

void FileLogAppender::start() {
    if (running_) {
        return;
    }

    running_ = true;
    thread_.reset(new Thread(std::bind(&FileLogAppender::consumeFunc, this), "file log"));
    thread_->start();
}

void FileLogAppender::stop() {
    if (!running_) {
        return;
    }
    running_.store(false, std::memory_order::release);
    cond_.notify_one();
    Thread::UniquePtr thread(std::move(thread_));
    if (thread) {
        thread->join();
    }
}

void FileLogAppender::consumeFunc() {
    NEMO_ASSERT(running_);
    BufferPtr newBuff1(new Buffer(EXEC_PAGESIZE, true));
    BufferPtr newBuff2(new Buffer(EXEC_PAGESIZE, true));
    BufferVector buffsToWrite;
    newBuff1->bzero();
    newBuff2->bzero();
    buffsToWrite.reserve(16);
    while (running_) {
        NEMO_ASSERT(newBuff1 && newBuff1->empty());
        NEMO_ASSERT(newBuff2 && newBuff2->empty());
        NEMO_ASSERT(buffsToWrite.empty());
        {
            std::unique_lock<std::mutex> uniqueLock(mutex_);
            if (buffers_.empty()) { // unusual usage!
                cond_.wait_for(uniqueLock, std::chrono::seconds(1));
            }
            buffers_.emplace_back(std::move(currBuff_));
            currBuff_ = std::move(newBuff1);
            if (!nextBuff_) {
                nextBuff_ = std::move(newBuff2);
            }
            buffsToWrite.swap(buffers_);
        }
    
        write(buffsToWrite);
        
        if (buffsToWrite.size() > 2) {
            // drop non-bzero-ed buffers, avoid trashing
            buffsToWrite.resize(2);
        }

        if (!newBuff1) {
            NEMO_ASSERT(!buffsToWrite.empty());
            newBuff1 = std::move(buffsToWrite.back());
            buffsToWrite.pop_back();
            newBuff1->clear();
        }

        if (!newBuff2) {
            NEMO_ASSERT(!buffsToWrite.empty());
            newBuff2 = std::move(buffsToWrite.back());
            buffsToWrite.pop_back();
            newBuff2->clear();
        }

        buffsToWrite.clear();
        flush();
    }
    {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        if (currBuff_ && !currBuff_->empty()) {
            buffers_.emplace_back(std::move(currBuff_));
        }
        if (nextBuff_ && !nextBuff_->empty()) {
            buffers_.emplace_back(std::move(nextBuff_));
        }
        buffsToWrite.swap(buffers_);
    }
    write(buffsToWrite);
    flush();
}

String FileLogAppender::toYamlString() {
    std::lock_guard<std::mutex> lockGuard(mutex_);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = filename_;
    node["level"] = LogLevel2String(level_);
    if(hasFormatter_ && formatter_) {
        node["formatter"] = formatter_->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

bool FileLogAppender::rollFile() {
    time_t now = 0;
    String newFilename = GetLogFileName(filename_, &now);
    time_t start = now / kRollPerSeconds * kRollPerSeconds;
    if (now > lastRoll_) {
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        fileAppender_.reset(new file_util::FileAppender(newFilename));
        return true;
    }

    return false;
}

} //namespace nemo