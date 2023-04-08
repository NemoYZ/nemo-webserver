#include "coroutine/routine_sync_timer.h"

#include <exception>
#include <algorithm>

#include "log/log.h"
#include "common/macro.h"

namespace nemo {
namespace coroutine {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

CallbackWrapper::CallbackWrapper(const Callback& cb) :
    mutex_(std::make_shared<std::mutex>()),
    cb_(cb) {
}

CallbackWrapper::CallbackWrapper(Callback&& cb) :
    mutex_(std::make_shared<std::mutex>()),
    cb_(std::move(cb)) {
}

CallbackWrapper::CallbackWrapper(CallbackWrapper&& other) :
    mutex_(std::move(other.mutex_)),
    cb_(std::move(other.cb_)),
    done_(other.done_) {
    bool otherCanceled = other.canceled_.load(std::memory_order::acquire);
    canceled_.store(otherCanceled, std::memory_order::release);
}

CallbackWrapper& CallbackWrapper::operator=(CallbackWrapper&& other) {
    if (this != &other) {
        mutex_ = std::move(other.mutex_);
        cb_ = std::move(other.cb_);
        done_ = other.done_;
        bool otherCanceled = other.canceled_.load(std::memory_order::acquire);
        canceled_.store(otherCanceled, std::memory_order::release);
    }

    return *this;
}

bool CallbackWrapper::invoke() {
    if (canceled_.load(std::memory_order_acquire)) {
        return false;
    }

    done_ = true;   // 先设置done, 为了reset中能重置状态
    try {
        cb_();
    } catch (const std::exception& e) {
        NEMO_LOG_ERROR(systemLogger) << "routine_sync_timer except: "
            << e.what();
    } catch (...) {
        NEMO_LOG_ERROR(systemLogger) << "routine_sync_timer except";
    }
    return true;
}

TimerId::TimerId(const TimePoint& tp, const Callback& cb) :
    tp_(tp),
    cbWrapper_(cb) {
}

TimerId::TimerId(const TimePoint& tp, Callback&& cb) :
    tp_(tp),
    cbWrapper_(std::move(cb)) {
}

TimerId::TimerId(TimerId&& other) :
    tp_(std::move(other.tp_)),
    mutex_(std::move(other.mutex_)),
    cbWrapper_(std::move(other.cbWrapper_)) {
}

TimerId& TimerId::operator=(TimerId&& other) {
    if (this != &other) {
        tp_ = std::move(other.tp_);
        mutex_ = std::move(other.mutex_);
        cbWrapper_ = std::move(other.cbWrapper_);
    }

    return *this;
}

bool RoutineSyncTimer::TimerIdCompare::operator()(const TimerId::SharedPtr& lhs, 
            const TimerId::SharedPtr& rhs) const {
    NEMO_ASSERT(lhs);
    NEMO_ASSERT(rhs);
    return *lhs < *rhs;
}

RoutineSyncTimer::RoutineSyncTimer() :
    stopped_(true) {
}

RoutineSyncTimer::~RoutineSyncTimer() {
    stop();
}

void RoutineSyncTimer::start() {
    if (!stopped_.load(std::memory_order::acquire)) {
        return;
    }

    stopped_.store(false, std::memory_order::release);
    NEMO_ASSERT(!thread_);
    thread_.reset(new Thread(std::bind(&RoutineSyncTimer::run, this), "RoutineSyncTimer"));
    thread_->start();
}

void RoutineSyncTimer::stop() {
    if (stopped_) {
            return;
    }

    stopped_.store(true, std::memory_order::release);
    Thread::UniquePtr thread;
    {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        cond_.notify_one();
        thread.swap(thread_);
    }
    NEMO_LOG_DEBUG(systemLogger) << "routine_sync_timer stopped";
    if (thread) {
        thread->join();
    }
}

TimerId::SharedPtr RoutineSyncTimer::add(const TimePoint& tp, const Callback& cb) {
    TimerId::SharedPtr id = std::make_shared<TimerId>(tp, cb);
    {
        std::unique_lock<std::mutex> unique_lock(mutex_);
        insertTimerId(id);
    }
    return id;
}

TimerId::SharedPtr RoutineSyncTimer::add(const TimePoint& tp, Callback&& cb) {
    TimerId::SharedPtr id = std::make_shared<TimerId>(tp, std::move(cb));
    {
        std::unique_lock<std::mutex> uniqueLock(mutex_);
        insertTimerId(id);
    }
    return id;
}

TimerId::SharedPtr 
RoutineSyncTimer::reset(const TimerId::SharedPtr& id, const TimePoint& tp) {
    TimerId::SharedPtr newId;
    if (eraseTimerId(id)) {
        newId = id;
        newId->setTimePoint(tp);
        newId->getCallbackWrapper().reset();
        std::unique_lock<std::mutex> uniqueLock(mutex_);
        insertTimerId(newId);
    }

    return newId;
}

bool RoutineSyncTimer::cancel(const TimerId::SharedPtr& id) {
    std::shared_ptr<std::mutex> invokeMutex = id->getCallbackWrapper().getMutex();
    {
        std::lock_guard<std::mutex> lockGuard(*invokeMutex);
        id->getCallbackWrapper().cancel();
    }
    std::lock_guard<std::mutex> lockGuard(mutex_);
    eraseTimerId(id);
    return id->getCallbackWrapper().done();
}

bool RoutineSyncTimer::atFront(const TimePoint& tp) {
    return timerIds_.empty() ? true : tp < (*timerIds_.begin())->getTimePoint();
}

bool RoutineSyncTimer::eraseTimerId(const TimerId::SharedPtr& target) {
    return timerIds_.erase(target) == 1;
}   

void RoutineSyncTimer::insertTimerId(const TimerId::SharedPtr& id) {
    TimePoint tp = id->getTimePoint();
    bool needNotify = atFront(tp);
    timerIds_.insert(id);
    if (needNotify && tp < nextCheckAbstime_) {
        cond_.notify_one();
    }
}

void RoutineSyncTimer::run() {
    NEMO_LOG_DEBUG(systemLogger) << "routine_sync_timer run";
    TimerId::SharedPtr id;
    std::unique_lock<std::mutex> uniqueLock(mutex_);
    while (!stopped_) {
        TimePoint nowTp = Now();
        if (!timerIds_.empty()) {
            id = *timerIds_.begin();
            NEMO_ASSERT(id);
            if (nowTp >= id->getTimePoint()) {
                std::shared_ptr<std::mutex> invokeMutex = id->getCallbackWrapper().getMutex();
                std::unique_lock<std::mutex> invokeUniqueLock(*invokeMutex, std::defer_lock);
                bool locked = invokeUniqueLock.try_lock();
                eraseTimerId(id);
                if (locked) {
                    uniqueLock.unlock();
                    id->getCallbackWrapper().invoke();
                    uniqueLock.lock();
                }
                continue;
            }
        }

        std::chrono::milliseconds sleepTime(1);
        if (id) {
            std::chrono::milliseconds delta = std::chrono::duration_cast<
                std::chrono::milliseconds>(id->getTimePoint() - nowTp);
            sleepTime = std::min(delta, sleepTime);
        } else {
            sleepTime = LoopInterval();
        }

        nextCheckAbstime_ = Now() + sleepTime;
        cond_.wait_for(uniqueLock, sleepTime);
    }
}

} // namespace coroutine
} // namespace nemo