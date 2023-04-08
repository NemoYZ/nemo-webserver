#include "coroutine/scheduler.h"

#include <thread>

#include "log/log.h"
#include "common/lexical_cast.h"
#include "common/config.h"
#include "common/macro.h"

namespace nemo {
namespace coroutine {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

struct SchedulerTimerStarter {
    SchedulerTimerStarter() {
        Scheduler::GetTimer()->start();
    }

    ~SchedulerTimerStarter() {
        Scheduler::GetTimer()->stop();
    }
};

static SchedulerTimerStarter schedulerTimerStarter;

void Scheduler::runBalance() {
    NEMO_LOG_DEBUG(systemLogger) << "balance thread start, scheduler_name="
                                << name_;
    while (NEMO_LIKELY(started_.load(std::memory_order::acquire))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        size_t processorCount = processors_.size();
        BlockMap blockings;
        ActiveMap actives;
        size_t activeTaskCount = 0;

        for (size_t i = 0; i < processorCount; ++i) {
            Processor* processor = processors_[i].get();
            // 等待中的processor不能算阻塞,无法加入新协程导致p饿死
            if (!processor->isWaiting() && processor->isBlocking()) {
                blockings.emplace(i, processor->runnableTaskCount());
                if (processor->active_) {
                    processor->active_ = false;
                }
            }
        }

        for (size_t i = 0; i < processorCount; ++i) {
            Processor* processor = processors_[i].get();
            size_t loadAverage = processor->runnableTaskCount();
            if (processor->active_) {
                actives.emplace(loadAverage, i);
                activeTaskCount += loadAverage;
                processor->mark();
            } else {
                if (!processor->isBlocking() || processor->isWaiting()) {
                    processor->active_ = true;
                    lastActiveProcessorIndex_.store(i, std::memory_order::release);
                }
            }
        }

        if (actives.empty()) {
            continue;
        }
        
        balanceBlock(blockings, actives);
        balanceActive(actives, activeTaskCount);
    }
}

void Scheduler::balanceBlock(BlockMap& blockings, ActiveMap& actives) {
    if(blockings.empty()) {
        return;
    }

    //将阻塞p的协程都steal出来
    TaskQueue blockingTasks;
    for (auto& [index, size] : blockings) {
        Processor* processor = processors_[index].get();
        blockingTasks.pushBackUnsafe(processor->steal(0));
    }
    
    if(blockingTasks.isEmptyUnsafe()) {
        return;
    }
   
    size_t totalBlockTasks = blockingTasks.size(); // 总协程数
    size_t lowerNum = 0; // 需要平分协程p的数量
    size_t avg = 0; // 平分的协程数
    
    ActiveMap::iterator lowerActive = actives.begin();
    for(; lowerActive != actives.end(); ++lowerActive) {
        totalBlockTasks += lowerActive->first;
        ++lowerNum;
        avg = totalBlockTasks / lowerNum;
        //p的负载大于avg则将它移除
        if(lowerActive->first > avg) {
            totalBlockTasks -= lowerActive->first;
            --lowerNum;
            avg = totalBlockTasks / lowerNum;
            break;
        }        
    }

    if(lowerActive != actives.end()) {
       ++lowerActive;
    }
    
    for(auto iter = actives.begin(); iter != lowerActive; ++iter) {
        TaskQueue in = blockingTasks.popFrontBulkUnsafe(avg - iter->first);
        if(in.isEmptyUnsafe()) {
            break;
        }
        Processor* processor = processors_[iter->second].get();
        processor->addTask(std::move(in));
    }
    //还剩下task就全都给最小的p
    if(!blockingTasks.isEmptyUnsafe()) {
        Processor* processor = processors_[actives.begin()->second].get();
        processor->addTask(std::move(blockingTasks));
    }
}

void Scheduler::balanceActive(ActiveMap& actives, size_t activeTaskCount) {
    size_t avg = activeTaskCount / actives.size();
     
    // 所有processor的task数都大于阈值
    if(actives.begin()->first > static_cast<size_t>(avg * kLoadBalanceRate)) {
        return;
    }
    
    TaskQueue tasks;
    for(auto iter = actives.rbegin(); iter != actives.rend(); ++iter) {
        if(iter->first <= avg) {
            break;
        }
        Processor* processor = processors_[iter->second].get();
        TaskQueue in = processor->steal(iter->first - avg); 
        tasks.pushBackUnsafe(std::move(in));
    }

    if(tasks.isEmptyUnsafe()) {
        return;
    }
    
    for(auto& [size, index] : actives) {
        if(size >= avg) {
            break;
        }
        Processor* processor = processors_[index].get();
        TaskQueue in = tasks.popFrontBulkUnsafe(avg - size);     
        processor->addTask(std::move(in));
    }
    //如果还剩下task,全都给最小的p
    if(!tasks.isEmptyUnsafe()) {
        Processor* processor = processors_[actives.begin()->second].get();
        processor->addTask(std::move(tasks));
    }
}

void Scheduler::createProcessor() {
    size_t processorId = processors_.size();
    String threadName = name_ + "'s processor" + LexicalCast<String>(processorId);
    processors_.emplace_back(std::make_unique<Processor>(this, processorId, taskOpt_));
    Processor* newProcessor = processors_.back().get();
    threads_.push_back(std::make_unique<Thread>([newProcessor](){
        newProcessor->process();
    }, threadName));
    threads_.back()->start();
}

Processor* Scheduler::nextTaskAcceptableProcessor() {
    // Processor* processor = Processor::GetCurrentProcessor();
    // if (processor && processor->active_ && this == processor->getScheduler()) {
    //     return processor;
    // }

    size_t index = lastActiveProcessorIndex_.load(std::memory_order::acquire);
    size_t processorCount = processors_.size();
    for (size_t i = 0; i < processorCount; ++i) {
        index = (index + 1) % processorCount;
        Processor* processor = processors_[index].get();
        if (processor->active_) {
            lastActiveProcessorIndex_.store(index, std::memory_order::release);
            return processor;
        }
    }

    return nullptr;
}

Processor* Scheduler::getTaskAcceptableProcessor(Task* task) {
    return nextTaskAcceptableProcessor();
}

Processor* Scheduler::getTaskAcceptableProcessor(const Callback& cb) {
    return nextTaskAcceptableProcessor();
}

Scheduler::Scheduler(StringArg name, int threadNumber) :
    taskOpt_(std::make_shared<SchedulerTaskOpt>(this)),
    name_(name),
    threadNumber_(0 == threadNumber ? Thread::HardwareConcurrency() : threadNumber) {
    processors_.reserve(threadNumber_);
    threads_.reserve(threadNumber_);
    processors_.emplace_back(std::make_unique<Processor>(this, 0, taskOpt_));
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (started_.load(std::memory_order::acquire)) {
        return;
    }
    started_.store(true, std::memory_order::release);

    for (int i = 1; i < threadNumber_; ++i) {
        createProcessor();
    }

    if (threadNumber_ > 1) {
        balanceThread_ = std::make_unique<Thread>([this](){
            this->runBalance();
        });
        balanceThread_->start();
    }

    processors_[0]->process();
}

void Scheduler::threadStart() {
    String threadName = name_ + "'s main processor";
    threads_.emplace_back(std::make_unique<Thread>([this](){
        this->start();
    }, threadName));
    threads_.back()->start();
}

void Scheduler::stop() {
    if (!started_.load(std::memory_order::acquire)) {
        return;
    }
    started_.store(false, std::memory_order::release);

    for (auto iter = processors_.begin(); iter != processors_.end(); ++iter) {
        Processor::UniquePtr processor;
        processor.swap(*iter);
        processor->notifiNewQueCondition();
    }

    processors_.clear();

    if (balanceThread_) {
        balanceThread_->join();
    }

    for (int i = 0; i < threadNumber_; ++i) {
        threads_[i]->join();
    }
    threads_.clear();
}

void Scheduler::addTask(Task&& task) {
    Processor* processor = getTaskAcceptableProcessor(&task);
    if (processor) {
        addTask(std::make_unique<Task>((std::move(task))));
    } else {
        NEMO_LOG_WARN(systemLogger) << "no acceptable processor, faild while addTask, "
                << "task_id=" << task.getId();
    }
}

void Scheduler::addTask(Task::UniquePtr&& task) {
    Processor* processor = getTaskAcceptableProcessor(task.get());
    if (processor) {
        processor->addTask(std::move(task));
    } else {
        NEMO_LOG_WARN(systemLogger) << "no acceptable processor, faild while addTask, "
                << "task_id=" << task->getId();
    }
}

void Scheduler::addTask(const Callback& cb) {
    Processor* processor = nextTaskAcceptableProcessor();
    if (processor) {
        processor->addTask(cb);
    } else {
        NEMO_LOG_WARN(systemLogger) << "no acceptable processor, addTask faild";
    }
}

void Scheduler::addTask(Callback&& cb) {
    Processor* processor = nextTaskAcceptableProcessor();
    if (processor) {
        processor->addTask(std::move(cb));
    } else {
        NEMO_LOG_WARN(systemLogger) << "no acceptable processor, addTask faild";
    }
}

void Scheduler::addTask(std::list<Runnable>&& tasks) {
    Processor* processor = nextTaskAcceptableProcessor();
    if (processor) {
        processor->addTask(std::move(tasks));
    } else {
        NEMO_LOG_WARN(systemLogger) << "no acceptable processor, addTask faild";
    }
}

void Scheduler::addTask(Processor::TaskQueue&& tasks) {
    Processor* processor = nextTaskAcceptableProcessor();
    if (processor) {
        processor->addTask(std::move(tasks));
    } else {
        NEMO_LOG_WARN(systemLogger) << "no acceptable processor, addTask faild";
    }
}

} //namespace coroutine
} //namespace nemo