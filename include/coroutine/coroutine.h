#pragma once

#include "common/singleton.h"
#include "coroutine/scheduler.h"

namespace nemo {
namespace coroutine {

class SyntaxHelper : public Singleton<SyntaxHelper> {
public:
    SyntaxHelper(Token);
    void operator+(const Scheduler::Callback& cb) {
        getScheduler()->addTask(cb);
    }
    void operator+(Scheduler::Callback&& cb) {
        getScheduler()->addTask(std::move(cb));
    }
    Scheduler::SharedPtr getScheduler() {
        return scheduler_;
    } 

private:
    Scheduler::SharedPtr scheduler_;
};

#define coroutine_scheduler nemo::coroutine::SyntaxHelper::GetInstance().getScheduler()

#define coroutine_async nemo::coroutine::SyntaxHelper::GetInstance()+

#define coroutine_start coroutine_scheduler->start()

#define coroutine_async_start coroutine_scheduler->threadStart()

} // namespace coroutine
} // namespace nemo