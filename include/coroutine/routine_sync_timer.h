#pragma once

#include <memory>
#include <atomic>
#include <functional>
#include <chrono>
#include <set>
#include <mutex>
#include <condition_variable>
#include <compare> //C++20

#include "common/noncopyable.h"
#include "common/thread.h"

namespace nemo {
namespace coroutine {

typedef std::function<void()> TimerCallback;
typedef std::chrono::steady_clock TimerClock;
typedef TimerClock::time_point TimerTimePoint;
typedef TimerClock::duration TimerDuration;

class Timer;

class CallbackWrapper : Noncopyable {
friend class Timer;
public:
    typedef std::shared_ptr<CallbackWrapper> SharedPtr;
    typedef std::unique_ptr<CallbackWrapper> UniquePtr;
    typedef TimerCallback Callback;

public:
    CallbackWrapper(const Callback& cb);
    CallbackWrapper(Callback&& cb);
    CallbackWrapper(CallbackWrapper&& other);
    CallbackWrapper& operator=(CallbackWrapper&& other);

    const Callback& getCallback() const {
        return cb_;
    }
    void setCallback(const Callback& cb) {
        cb_ = cb;
        canceled_.store(false, std::memory_order_release);
        reset();
    }
    void setCallback(Callback&& cb) {
        cb_ = std::move(cb);
        canceled_.store(false, std::memory_order_release);
        reset();
    }
    void reset() { done_ = false; }
    std::shared_ptr<std::mutex> getMutex() { 
        return mutex_; 
    }
    bool invoke();
    void cancel() {
        canceled_.store(false, std::memory_order_release);
    }
    bool done() const { return done_; }

private:
    std::shared_ptr<std::mutex> mutex_;
    Callback cb_;
    std::atomic<bool> canceled_ {false};
    bool done_ {false};
};

class TimerId : Noncopyable {
friend class Timer;
public:
    typedef std::unique_ptr<TimerId> UniquePtr;
    typedef std::shared_ptr<TimerId> SharedPtr;
    typedef TimerCallback Callback;
    typedef TimerClock Clock;
    typedef TimerTimePoint TimePoint;
    typedef TimerDuration Duration;

public:
    TimerId() = default;
    TimerId(const TimePoint& tp, const Callback& cb);
    TimerId(const TimePoint& tp, Callback&& cb);
    TimerId(TimerId&& other);
    ~TimerId() = default;
    std::strong_ordering operator<=>(const TimerId& other) const noexcept {
        return tp_ <=> other.tp_;
    }
    TimerId& operator=(TimerId&& other);

    const TimePoint& getTimePoint() const { return tp_; }
    CallbackWrapper& getCallbackWrapper() { return cbWrapper_; }
    const std::shared_ptr<std::mutex>& getMutex() const { return mutex_; }
    void setTimePoint(const TimePoint& tp) { tp_ = tp; }

private:
    TimePoint tp_;
    std::shared_ptr<std::mutex> mutex_;
    CallbackWrapper cbWrapper_;
};

class RoutineSyncTimer : Noncopyable {
public:
    typedef std::shared_ptr<RoutineSyncTimer> SharedPtr;
    typedef std::unique_ptr<RoutineSyncTimer> UniquePtr;
    typedef TimerCallback Callback;
    typedef TimerClock Clock;
    typedef TimerTimePoint TimePoint;
    typedef TimerDuration Duration;

public:
    RoutineSyncTimer();
    ~RoutineSyncTimer();

    void start();
    void stop();
    TimerId::SharedPtr add(const TimePoint& tp, const Callback& cb);
    TimerId::SharedPtr add(const TimePoint& tp, Callback&& cb);
    TimerId::SharedPtr add(const std::chrono::milliseconds& after, const Callback& cb) {
        return add(Now() + after, cb);
    }
    TimerId::SharedPtr add(const std::chrono::milliseconds& after, Callback&& cb) {
        return add(Now() + after, std::move(cb));
    }
    TimerId::SharedPtr reset(const TimerId::SharedPtr& id, const TimePoint& tp);
    bool cancel(const TimerId::SharedPtr& id);

public:
    static TimePoint* NullTimePoint() { 
        return static_cast<TimePoint*>(nullptr); 
    }
    static TimePoint Now() { return Clock::now(); }
    static std::chrono::milliseconds LoopInterval() { 
        return std::chrono::milliseconds(20);
    }

private:
    struct TimerIdCompare {
        bool operator()(const TimerId::SharedPtr& lhs, 
            const TimerId::SharedPtr& rhs) const;
    };
    typedef std::set<TimerId::SharedPtr, TimerIdCompare> TimerIdSet;

private:
    bool atFront(const TimePoint& tp);
    bool eraseTimerId(const TimerId::SharedPtr& target);
    void insertTimerId(const TimerId::SharedPtr& id);
    void run();

private:
    Thread::UniquePtr thread_;
    TimerIdSet timerIds_;
    TimePoint nextCheckAbstime_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> stopped_;
};

} // namespace coroutine
} // namespace nemo