#pragma once

#include <memory>
#include <map>
#include <atomic>
#include <mutex>
#include <vector>

#include "common/noncopyable.h"
#include "common/thread.h"
#include "coroutine/processor.h"

namespace nemo {
namespace coroutine {

class Scheduler : Noncopyable {
friend class Processor;
public:
    typedef std::shared_ptr<Scheduler> SharedPtr;
    typedef std::unique_ptr<Scheduler> UniquePtr;
    typedef Task::Callback Callback;
    typedef Processor::TaskQueue TaskQueue;
    typedef Processor::Runnable Runnable;
    typedef RoutineSyncTimer TimerType;

public:
    constexpr static float kLoadBalanceRate = 0.01;

public:
    static TimerType* GetTimer() {
        static TimerType timer;
        return &timer;
    }

public:
    Scheduler(StringArg name = "", int threadNumber = Thread::HardwareConcurrency());
    ~Scheduler();

    bool isStop() const { return !started_.load(std::memory_order::acquire); }
    bool isEmpty() const { return 0 == taskCount_; }
    void start();
    void threadStart();
    void stop();
    void addTask(Task&& task);
    void addTask(Task::UniquePtr&& task);
    void addTask(const Callback& cb);
    void addTask(Callback&& cb);
    template<std::input_iterator InputIter>
    void addTask(InputIter first, InputIter last);
    void addTask(std::list<Runnable>&& tasks);
    void addTask(TaskQueue&& tasks);
    uint64_t taskCount() const { return taskCount_; }

private:
    typedef std::vector<Processor::UniquePtr>::size_type IndexType;
    typedef std::map<IndexType, size_t> BlockMap;   // 在vector中的下标和协程数的映射
    typedef std::multimap<size_t, IndexType> ActiveMap; //协程数到deque中下标的映射

private:
    class SchedulerTaskOpt;

private:
    // for dispatch thread
    void runBalance();

    void balanceBlock(BlockMap& blockings, ActiveMap& actives);
    void balanceActive(ActiveMap& actives, size_t activeTaskCount);
    void createProcessor();
    Processor* nextTaskAcceptableProcessor();
    Processor* getTaskAcceptableProcessor(Task* task);
    Processor* getTaskAcceptableProcessor(const Callback& cb);

private:
    std::atomic<size_t> lastActiveProcessorIndex_;
    std::atomic<uint64_t> taskCount_;
    Thread::UniquePtr balanceThread_;
    std::shared_ptr<Processor::TaskOptCallback> taskOpt_;
    std::vector<Processor::UniquePtr> processors_;
    std::vector<Thread::UniquePtr> threads_;
    String name_;
    const int threadNumber_;
    std::atomic<bool> started_{false};
};

class Scheduler::SchedulerTaskOpt : public Processor::TaskOptCallback {
public:
    SchedulerTaskOpt(void* arg) :
        Processor::TaskOptCallback(arg) {
    }

public:
    void onAdd(Task* task) override {
        Scheduler* scheduler = static_cast<Scheduler*>(arg_);
        scheduler->taskCount_.fetch_add(1, std::memory_order::relaxed);
    }
    void onErase(Task* task) override {
        Scheduler* scheduler = static_cast<Scheduler*>(arg_);
        scheduler->taskCount_.fetch_sub(1, std::memory_order::relaxed);
    }
};

template<std::input_iterator InputIter>
void Scheduler::addTask(InputIter first, InputIter last) {
    for (; first != last; ++first) {
        addTask(*first);
    }
}

} //namespace coroutine
} //namespace nemo