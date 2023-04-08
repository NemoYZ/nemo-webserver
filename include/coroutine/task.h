#pragma once

#include <stdint.h>

#include <memory>
#include <functional>
#include <type_traits>

#include "coroutine/routine_sync_timer.h"
#include "context/context.h"
#include "common/types.h"
#include "common/lexical_cast.h"
#include "common/noncopyable.h"

namespace nemo {
namespace coroutine {

class Processor;

class Task : Noncopyable {
friend class Processor;
public:
    typedef std::shared_ptr<Task> SharedPtr;   
    typedef std::unique_ptr<Task> UniquePtr;
    typedef std::function<void()> Callback;

    enum State : int8_t {
        READY,
        RUNNING,
        BLOCK,
        DONE,
        EXCEPT,
        UNKNOWN,
    };

    static const char* State2String(State state);
    static State String2State(const String& str);

public:
    Task(const Callback& cb);
    Task(Callback&& cb);
    Task(Task&& other) noexcept;
    ~Task() = default;

    Task& operator=(Task&& other) noexcept;

    void swapIn() {
        SetCurrentTask(this);
        ctx_.SwapIn();
    }

    void swapOut() {
        SetCurrentTask(nullptr);
        ctx_.SwapOut();
    }

    uint64_t getId() const { return id_; }

    State getState() const { return state_; }

    Processor* getProcessor() const { return processor_; }

    void reset(const Callback& cb) {
        cb_ = cb;
    }

    void reset(Callback&& cb) {
        cb_ = std::move(cb);
    }

    void reset() {
        cb_ = Callback();
    }

public:
    static Task* GetCurrentTask();

private:
    static void SetCurrentTask(Task* task);
    static void Run(intptr_t vp);
    
private:
    uint64_t id_;
    Processor* processor_;
    TimerId::SharedPtr suspendTimerId_;
    RoutineSyncTimer* scheduleTimer_;
    Context ctx_;
    Callback cb_;
    State state_;
};

template<typename T>
struct is_runnable {
    static constexpr bool value = 
        std::is_same_v<typename std::decay<T>::type, Task::Callback> ||
        std::is_same_v<typename std::decay<T>::type, Task> ||
        std::is_same_v<typename std::decay<T>::type, Task::UniquePtr>;
};

template<typename T>
inline constexpr bool is_runnable_v = is_runnable<T>::value;

template<typename T>
concept runnable = is_runnable_v<T>;


} //namespace coroutine

template<>
inline String LexicalCast<String, coroutine::Task::State>(
        const coroutine::Task::State& state) {
    return coroutine::Task::State2String(state);
}

template<>
inline coroutine::Task::State LexicalCast<coroutine::Task::State, String>(
        const String& str) {
    return coroutine::Task::String2State(str);
}

} //namespace nemo