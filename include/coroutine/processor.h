#pragma once

#include <memory>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <variant> //C++17
#include <concepts> //C++20

#include "coroutine/task.h"
#include "container/concurrent_linked_deque.h"

namespace nemo {
namespace coroutine {

class Scheduler;

class Processor {
friend class Scheduler;
public:
    class Runnable;
    class SuspendEntry;
    class TaskOptCallback;
    
public:
    typedef std::shared_ptr<Processor> SharedPtr;
    typedef std::unique_ptr<Processor> UniquePtr;
    typedef Task::Callback Callback;
    typedef ConcurrentLinkedDeque<Runnable> TaskQueue;

private:
    struct TaskPointerDeleter {
        TaskPointerDeleter(Processor* proc) :
            processor(proc) {
        }
        /**
         * @brief task智能指针删除类
         * @param task 
         * @attention 我们删除的task一定是在runQue_中取出来的，
         *          在waitSet_中的task要么是在stop操作之后删除
         *          要么就wakeup加到runQue_中被删除
         *          所以在删除的时候我们什么都不做
         */
        void operator()(Task* task) {
            if (processor->stop_) {
                delete task;
            } else {
                processor->runQue_.emplaceBackUnsafe(std::unique_ptr<Task>(task));
            }
        }

        Processor* processor{nullptr};
    };
    typedef std::shared_ptr<Task> TaskSharedPtr;
    typedef std::unordered_set<TaskSharedPtr> TaskWaitSet;

public:
    // for scheduler
    Processor(Scheduler* scheduler, size_t id, const std::shared_ptr<TaskOptCallback>& taskOpt);

    Processor(Scheduler* scheduler, const std::shared_ptr<TaskOptCallback>& opt = nullptr);
    ~Processor();

    pid_t getId() const { return id_; }
    Scheduler* getScheduler() const { return scheduler_; }
    TaskOptCallback* getTaskOpt() const { return taskOpt_.get(); }
    size_t runnableTaskCount() const { return runQue_.size() + newQue_.size(); }
    void addTask(Task&& task);
    void addTask(Task::UniquePtr&& task);
    void addTask(const Callback& cb);
    void addTask(Callback&& cb);
    template<std::input_iterator InputIter>
    void addTask(InputIter first, InputIter last);
    void addTask(std::list<Runnable>&& tasks);
    void addTask(ConcurrentLinkedDeque<Runnable>&& tasks);
    void process();

public:
    static Processor* GetCurrentProcessor();
    static Task* GetCurrentRunningTask();
    [[nodiscard]] static SuspendEntry Suspend();
    static SuspendEntry Suspend(std::chrono::steady_clock::duration dur);
    static SuspendEntry Suspend(std::chrono::steady_clock::time_point timePoint);
    static bool WakeUp(const SuspendEntry& suspendEntry);
    static bool IsExpired(const SuspendEntry& suspendEntry);
    static void Yield();

private:
    void setTaskOpt(const std::shared_ptr<TaskOptCallback>& taskOpt) {
        taskOpt_ = taskOpt;
    }
    void mark();

    void waitNewQueCondition();
    void notifiNewQueCondition();
    bool getFrontTask(Task::UniquePtr&& task);
    void addNewTask();
    bool isWaiting() { return waitting_; }
    bool isBlocking();
    TaskQueue steal(size_t n);
    SuspendEntry suspendBySelf(Task* task);
    bool wakeUpBySelf(const TaskSharedPtr& task);

private:
    static void SetCurrentProcessor(Processor* processor);

private:
    Scheduler* scheduler_;
    uint64_t markTickMillionSeconds_{0};
    uint64_t markSwitch_{0};
    uint64_t switchCount_{0};
    size_t id_{static_cast<size_t>(-1)};
    Task::UniquePtr runningTask_;
    Task::UniquePtr nextTask_;
    std::shared_ptr<TaskOptCallback> taskOpt_;
    TaskQueue runQue_;
    TaskWaitSet waitSet_;
    TaskQueue newQue_;
    std::mutex mutex_;
    std::mutex waitSetMutex_;
    std::condition_variable newQueCond_;
    std::atomic<bool> waitting_{false};
    bool active_{true};
    bool notified_{false};
    bool stop_{false};
};

class Processor::Runnable : Noncopyable {
public:
    typedef Processor::Callback Callback;

public:
    Runnable() = default;
    Runnable(Runnable&& other);
    Runnable(Task::UniquePtr&& task);
    Runnable(const Callback& cb);
    Runnable(Callback&& cb);
    ~Runnable() = default;

    Runnable& operator=(Task::UniquePtr&& task) {
        runner_ = std::move(task);
        return *this;
    }
    Runnable& operator=(const Callback& cb) {
        runner_ = cb;
        return *this;
    }
    Runnable& operator=(Callback&& cb) {
        runner_ = std::move(cb);
        return *this;
    }
    Runnable& operator=(Runnable&& other) {
        if (this != &other) {
            runner_ = std::move(other.runner_);
        }
        return *this;
    }

    Task::UniquePtr get() {
        Task::UniquePtr task;
        if (std::holds_alternative<Task::UniquePtr>(runner_)) {
            task = std::move(std::get<0>(runner_));
        } else if (std::holds_alternative<Callback>(runner_)) {
            task = std::make_unique<Task>(std::get<1>(runner_));
        }
        return task;
    }

    void set(Task::UniquePtr&& task) {
        runner_ = std::move(task);
    }
    void set(const Callback& cb) {
        runner_ = cb;
    }
    void set(Callback&& cb) {
        runner_ = std::move(cb);
    }

private:
    std::variant<Task::UniquePtr, Callback> runner_;
};

class Processor::SuspendEntry {
friend class Processor;
public:
    SuspendEntry() = default;
    SuspendEntry(const Task::SharedPtr& task);
    bool operator==(const SuspendEntry& other) const {
        return id_ == other.id_;
    }
    bool operator<(const SuspendEntry& other) const {
        if (id_ == other.id_) {
            return task_.lock().get() < other.task_.lock().get();
        }
        return id_ < other.id_;
    }
    operator bool() const { return !!(task_.lock().get()); }
    bool isExpired() const { return task_.expired(); }

private:
    uint64_t id_;
    std::weak_ptr<Task> task_;
};

class Processor::TaskOptCallback {
public:
    TaskOptCallback(void* arg) :
        arg_(arg) {
    }

public:
    virtual void onAdd(Task* task) {}
    virtual void onRun(Task* task) {}
    virtual void onBlock(Task* task) {}
    virtual void onErase(Task* task) {}
    virtual void onWakeUp(Task* task) { 
        task->scheduleTimer_ = nullptr;
    }

protected:
    void* arg_{nullptr};
};

template<std::input_iterator InputIter>
void Processor::addTask(InputIter first, InputIter last) {
    using VType = typename std::iterator_traits<InputIter>::value_type;
    static_assert(is_runnable_v<VType>, "runnable object required");

    taskOpt_->onAdd(nullptr);

    for (; first != last; ++first) {
        if constexpr(std::is_same_v<std::decay<VType>, Task>) {
            newQue_.emplaceBack(new Task(std::move(*first)));
        } else {
            newQue_.emplaceBack(std::move(*first));
        }
    }
}

} //namespace coroutine
} //namespace nemo