#pragma once 

#include <time.h>
#include <sys/time.h>

#include <algorithm>
#include <compare> //C++20

#include "common/lexical_cast.h"
#include "common/types.h"

namespace nemo {

class Timestamp {
public:
    constexpr static int64_t kMicroSecondsPerSecond = 1000 * 1000;
    constexpr static int64_t kMillionSecondsPerSecond = 1000;

public:
    Timestamp(int64_t microSecondsSinceEpoch) :
        microSecondsSinceEpoch_(microSecondsSinceEpoch) {
    }
    void swap(Timestamp& other) {
        if (this != &other) {
            std::swap(microSecondsSinceEpoch_, other.microSecondsSinceEpoch_);
        }
    }
    
    time_t secondsSinceEpoch() const { 
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond); 
    }

    int64_t microSecondsSinceEpoch() const { 
        return microSecondsSinceEpoch_; 
    }

    Timestamp& operator=(const Timestamp& other) {
        if (this != &other) { //不用判断也能运行
            microSecondsSinceEpoch_ = other.microSecondsSinceEpoch_;
        }
        return *this;
    }

    Timestamp& operator=(time_t microSeconds) {
        microSecondsSinceEpoch_ = microSeconds;
        return *this;
    }

    /**
     * @brief 
     * @attention C++20 required
     */
    std::strong_ordering operator<=>(const Timestamp& other) {
        return microSecondsSinceEpoch_ <=> other.microSecondsSinceEpoch_;
    }

    String toFormattedString(StringArg format) const {
        char buf[64]{0};
        time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
        struct tm time;
        ::localtime_r(&seconds, &time);
        ::strftime(buf, sizeof(buf), format.data(), &time);
        return buf;
    }

public:
    static Timestamp Now() { 
        struct timeval tv;
        ::gettimeofday(&tv, nullptr);
        int64_t seconds = tv.tv_sec;
        return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
    }

private:
    int64_t microSecondsSinceEpoch_;
};

} //namespace nemo