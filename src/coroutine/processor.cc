#include "coroutine/processor.h"

#include "coroutine/scheduler.h"
#include "common/macro.h"
#include "log/log.h"
#include "util/util.h"
#include "net/io/hook.h"

namespace nemo {
namespace coroutine {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");
static thread_local Processor* currentProcessor{nullptr};

Processor::Runnable::Runnable(Runnable&& other) :
    runner_(std::move(other.runner_)) {
}

Processor::Runnable::Runnable(Task::UniquePtr&& task) :
    runner_(std::move(task)){
}

Processor::Runnable::Runnable(const Callback& cb) :
    runner_(cb) {
}

Processor::Runnable::Runnable(Callback&& cb) :
    runner_(std::move(cb)){
}

Processor::SuspendEntry::SuspendEntry(const Task::SharedPtr& task) :
    task_(task) {
}

Processor* Processor::GetCurrentProcessor() {
    return currentProcessor;
}

void Processor::SetCurrentProcessor(Processor* processor) {
    currentProcessor = processor;
}

Task* Processor::GetCurrentRunningTask() {
    Processor* processor = GetCurrentProcessor();
    return processor ? processor->runningTask_.get() : nullptr;
}

Processor::SuspendEntry Processor::Suspend() {
    Task* task = GetCurrentRunningTask();
    NEMO_ASSERT(task);
    NEMO_ASSERT(task->processor_);
    return task->processor_->suspendBySelf(task);
}

Processor::SuspendEntry Processor::Suspend(std::chrono::steady_clock::duration dur) {
    return Suspend(std::chrono::steady_clock::now() + dur);
}

Processor::SuspendEntry Processor::Suspend(std::chrono::steady_clock::time_point timePoint) {
    SuspendEntry entry = Suspend();
    Task* task = GetCurrentRunningTask();
    if (task->scheduleTimer_) {
        task->scheduleTimer_->cancel(task->suspendTimerId_);
        task->scheduleTimer_ = nullptr;
    }

    task->scheduleTimer_ = Scheduler::GetTimer();
    task->suspendTimerId_ = task->scheduleTimer_->add(timePoint, [entry, task]() mutable {
        Processor::WakeUp(entry);
    });
    return entry;
}

bool Processor::WakeUp(const SuspendEntry& suspendEntry) {
    TaskSharedPtr task = suspendEntry.task_.lock();
    if (!task) {
        NEMO_LOG_ERROR(systemLogger) << "weak up fail";
        return false;
    }

    Processor* processor = task->getProcessor();
    return processor ? processor->wakeUpBySelf(task) : false;
}

bool Processor::IsExpired(const SuspendEntry& suspendEntry) {
    return suspendEntry.isExpired();
}

void Processor::Yield() {
    Task* task = GetCurrentRunningTask();
    NEMO_ASSERT(task);
    task->swapOut();
}

Processor::Processor(Scheduler* scheduler, const std::shared_ptr<TaskOptCallback>& opt) :
    scheduler_(scheduler),
    id_(static_cast<size_t>(-1)),
    taskOpt_(opt) {
    if (!taskOpt_) {
        taskOpt_ = std::make_shared<TaskOptCallback>(this);
    }
    net::io::SetHookEnable(true);
}

Processor::Processor(Scheduler* scheduler, size_t id, const std::shared_ptr<TaskOptCallback>& opt) :
    scheduler_(scheduler),
    id_(id),
    taskOpt_(opt) {
    if (!taskOpt_) {
        taskOpt_ = std::make_shared<TaskOptCallback>(this);
    }
    net::io::SetHookEnable(true);
}

Processor::~Processor() {
    stop_ = true;
}

void Processor::addTask(Task&& task) {
    Task::UniquePtr tk = std::make_unique<Task>(std::move(task));
    addTask(std::move(tk));
}

void Processor::addTask(Task::UniquePtr&& task) {
    taskOpt_->onAdd(nullptr);
    std::lock_guard<std::mutex> lockGuard(newQue_.getMutext());
    newQue_.emplaceBackUnsafe(std::move(task));
    if (waitting_) {
        newQueCond_.notify_all();
    } else {
        notified_ = true;
    }
}

void Processor::addTask(const Callback& cb) {
    taskOpt_->onAdd(nullptr);
    std::lock_guard<std::mutex> lockGuard(newQue_.getMutext());
    newQue_.emplaceBackUnsafe(cb);
    if (waitting_) {
        newQueCond_.notify_all();
    } else {
        notified_ = true;
    }
}

void Processor::addTask(Callback&& cb) {
    taskOpt_->onAdd(nullptr);
    std::lock_guard<std::mutex> lockGuard(newQue_.getMutext());
    newQue_.emplaceBackUnsafe(std::move(cb));
    if (waitting_) {
        newQueCond_.notify_all();
    } else {
        notified_ = true;
    }
}

void Processor::addTask(std::list<Runnable>&& tasks) {
    taskOpt_->onAdd(nullptr);
    std::lock_guard<std::mutex> lockGuard(newQue_.getMutext());
    newQue_.pushBackUnsafe(TaskQueue(std::move(tasks)));
    if (waitting_) {
        newQueCond_.notify_all();
    } else {
        notified_ = true;
    }
}

void Processor::addTask(ConcurrentLinkedDeque<Runnable>&& tasks) {
    taskOpt_->onAdd(nullptr);
    std::lock_guard<std::mutex> lockGuard(newQue_.getMutext());
    newQue_.pushBackUnsafe(std::move(tasks));
    if (waitting_) {
        newQueCond_.notify_all();
    } else {
        notified_ = true;
    }
}

void Processor::mark() {
    if (runningTask_ && markSwitch_ != switchCount_) {
        markSwitch_ = switchCount_;
        markTickMillionSeconds_ = GetCurrentMillionSeconds();
    }
}

void Processor::waitNewQueCondition() {
    std::unique_lock<std::mutex> uniqueLock(newQue_.getMutext());
    if (notified_) {
        notified_ = false;
    } else {
        waitting_ = true;
        newQueCond_.wait(uniqueLock);
        waitting_ = false;
    }
}

void Processor::notifiNewQueCondition() {
    std::unique_lock<std::mutex> uniqueLock(newQue_.getMutext());
    if (waitting_) {
        newQueCond_.notify_all();
    } else {
        notified_ = true;
    }
}

bool Processor::getFrontTask(Task::UniquePtr&& task) {
    return false;
}

void Processor::addNewTask() {
    runQue_.pushBack(newQue_.popAll());
}

bool Processor::isBlocking() {
    if (!markSwitch_ || (switchCount_ != markSwitch_)) {
        return false;
    }

    return GetCurrentMillionSeconds() > markTickMillionSeconds_ + 100 * 1000;
}

Processor::TaskQueue Processor::steal(size_t n) {
    TaskQueue result;
    if (n > 0) {
        result.pushBackUnsafe(newQue_.popBackBulk(n));
        if (result.size() < n) {
            result.pushBackUnsafe(runQue_.popBackBulk(n - result.size()));
        }
    } else {
        // 直接move(newQue_)也可正常运行，但是后面再访问newQue_将是未定义行为
        result.pushBackUnsafe(newQue_.popAll());
    }
    
    return result;
}

Processor::SuspendEntry Processor::suspendBySelf(Task* task) {
    NEMO_ASSERT(runningTask_.get() == task);
    NEMO_ASSERT(task->state_ == Task::State::RUNNING);

    TaskSharedPtr taskPtr(task, TaskPointerDeleter(this));
    taskPtr->state_ = Task::State::BLOCK;
    NEMO_LOG_DEBUG(systemLogger) << "task blocked, id=" << task->getId();
    {
        std::lock_guard<std::mutex> lockGuard(waitSetMutex_);
        waitSet_.emplace(taskPtr);
    }

    Runnable run;
    if (runQue_.popFront(run)) {
        nextTask_ = run.get();
    }
    
    return SuspendEntry(taskPtr);
}

bool Processor::wakeUpBySelf(const TaskSharedPtr& task) {
    Task* wakeUpTask = task.get();

    std::unique_lock<std::mutex> uniqueLock(mutex_);
    taskOpt_->onWakeUp(wakeUpTask);
    
    if (wakeUpTask->scheduleTimer_) {
        wakeUpTask->scheduleTimer_->cancel(wakeUpTask->suspendTimerId_);
        wakeUpTask->scheduleTimer_ = nullptr;
    }

    // 注意，这里不会delete task所拥有的指针
    waitSet_.erase(task);
    
    if (1 == runQue_.sizeUnsafe() || GetCurrentProcessor() != this) {
        uniqueLock.unlock();
        notifiNewQueCondition();
    }

    return true;
}

void Processor::process() {
    SetCurrentProcessor(this);
    Runnable run;

    while (scheduler_ && !scheduler_->isStop()) {
        // 先取得要运行的对象
        if (!runQue_.popFront(run)) {
            addNewTask();
            if (!runQue_.popFront(run)) {
                NEMO_LOG_DEBUG(systemLogger) << "processor waitting, id=" << id_;
                waitNewQueCondition();
                addNewTask();
                continue;
            }
        }
        runningTask_ = run.get();
        if (!runningTask_) {
            NEMO_LOG_ERROR(systemLogger) << "faild to get runnable";
        }

        // 已经取得，开始调度
        // 先来先服务(FCFS)
        while (runningTask_ && scheduler_ && !scheduler_->isStop()) {
            runningTask_->processor_ = this;
            runningTask_->state_ = Task::State::RUNNING;
            Task::SetCurrentTask(runningTask_.get());
            taskOpt_->onRun(runningTask_.get());
            ++switchCount_;
            runningTask_->swapIn();
            switch (runningTask_->state_) {
                case Task::State::RUNNING: 
                    if (runQue_.isEmpty()) {
                        addNewTask();
                    }
                    runQue_.emplaceBack(std::move(runningTask_));
                    if (runQue_.popFront(run)) {
                        runningTask_ = run.get();
                    } else {
                        runningTask_ = Task::UniquePtr(nullptr);
                    }
                    break;
                case Task::State::BLOCK:
                    taskOpt_->onBlock(runningTask_.get());
                    // suspend函数会用shared_ptr接管runningTask的生命周期
                    // 这里release是没问题的
                    runningTask_.release();
                    runningTask_ = std::move(nextTask_);
                    nextTask_ = Task::UniquePtr(nullptr);
                    break;
                case Task::State::EXCEPT:
                    [[fallthrough]];
                case Task::State::DONE:  
                    [[fallthrough]];  
                default:
                    if (runQue_.isEmpty()) {
                        addNewTask();
                    }
                    taskOpt_->onErase(runningTask_.get());
                    NEMO_LOG_DEBUG(systemLogger) << "erase task, task_id="
                            << runningTask_->getId()
                            << " task_state="
                            << Task::State2String(runningTask_->getState());
                    if (newQue_.popFront(run)) {
                        runningTask_ = run.get();
                    } else {
                        runningTask_ = std::move(nextTask_);
                        nextTask_ = Task::UniquePtr(nullptr);
                    }
                    break;
            }
        }
    }   
}

} //namespace coroutine
} //namespace nemo