#include "coroutine/task.h"
#include <atomic>

#include "log/log.h"
#include "util/util.h"
#include "coroutine/processor.h"
#include "common/macro.h"

namespace nemo {
namespace coroutine {

static thread_local Task* gCurrentTask{nullptr};

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");
static std::atomic<size_t> gTaskId{1};

const char* Task::State2String(State state) {
#define CASE(name)  \
    case State::name: \
        return #name
    switch (state) {
        CASE(READY);
        CASE(RUNNING);
        CASE(BLOCK);
        CASE(EXCEPT);
        CASE(DONE);
    default:
        return "UNKNOWN";
        break;
    }
    return "UNKNOWN";
}

Task::State Task::String2State(const String& str) {
#define IF(state, v)  \
    do {                \
        if(strncasecmp(#state, v.data(), v.size()) == 0) { \
            return Task::state; \
        } \
    } while (0)
    IF(READY, str);
    IF(RUNNING, str);
    IF(BLOCK, str);
    IF(EXCEPT, str);
    IF(DONE, str);

    return State::UNKNOWN;
}

Task* Task::GetCurrentTask() {
    return gCurrentTask;
}

void Task::SetCurrentTask(Task* task) {
    gCurrentTask = task;
}

void Task::Run(intptr_t vp) {
    Task* task = reinterpret_cast<Task*>(vp);
    try {
        task->state_ = State::RUNNING;
        task->cb_();
        task->state_ = State::DONE;
        task->reset();
    } catch(const std::exception& e) {
        task->state_ = State::EXCEPT;
        NEMO_LOG_ERROR(systemLogger) << "Task Except: " << e.what()
            << " task_id=" << task->getId()
            << "\n"
            << nemo::BacktraceToString();
    } catch(...) {
        task->state_ = State::EXCEPT;
        NEMO_LOG_ERROR(systemLogger) << "Task Except"
            << " task_id=" << task->getId()
            << "\n"
            << nemo::BacktraceToString();
    }

    task->swapOut();
    NEMO_LOG_FATAL(systemLogger) << "never reached";
}

Task::Task(const Callback& cb) :
    id_(gTaskId),
    processor_(nullptr),
    scheduleTimer_(nullptr),
    ctx_(&Task::Run, reinterpret_cast<intptr_t>(this), Context::kStackSize),
    cb_(cb),
    state_(State::READY) {
    NEMO_ASSERT(cb_);
    gTaskId.fetch_add(1, std::memory_order::acquire);
}

Task::Task(Callback&& cb) :
    id_(gTaskId),
    processor_(nullptr),
    scheduleTimer_(nullptr),
    ctx_(&Task::Run, reinterpret_cast<intptr_t>(this), Context::kStackSize),
    cb_(std::move(cb)),
    state_(State::READY) {
    NEMO_ASSERT(cb_);
    gTaskId.fetch_add(1, std::memory_order::acquire);
}

Task::Task(Task&& other) noexcept :
    id_(other.id_),
    processor_(other.processor_),
    ctx_(std::move(other.ctx_)),
    cb_(std::move(other.cb_)),
    state_(other.state_) {
    //other.id_ = 0;
    //other.processor = nullptr;
}

Task& Task::operator=(Task&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        processor_ = other.processor_;
        ctx_ = std::move(other.ctx_);
        cb_ = std::move(other.cb_);
        state_ = other.state_;
        //other.id_ = 0;
        //other.processor = nullptr;
    }

    return *this;
}

} //namespace coroutine
} //namespace nemo