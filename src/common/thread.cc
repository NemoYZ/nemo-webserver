#include "common/thread.h"

#include <errno.h>

#include "common/macro.h"
#include "log/log.h"
#include "util/util.h"

namespace nemo {

static thread_local Thread* currentThread = nullptr;
static thread_local String currentThreadName = "UNKNOW";

static nemo::Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

Thread* Thread::GetCurrentThread() {
    return currentThread;
}

const String& Thread::GetCurrentThreadName() {
    return currentThreadName;
}

void Thread::SetCurrentThreadName(StringArg name) {
    if (name.empty()) {
        return;    
    }
    if (currentThread) {
        currentThread->name_.assign(name.data(), name.size());
    }
    currentThreadName = name;
}

long Thread::HardwareConcurrency() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

Thread::Thread(Callback&& cb, StringArg name) :
    thread_(0),
    cb_(std::move(cb)),
    name_(name.data(), name.size()) {
    if(name_.empty()) {
        name_ = "UNKNOW";
    }
}

Thread::Thread(const Callback& cb, StringArg name) :
    thread_(0),
    cb_(cb),
    name_(name.data(), name.size()) {
    if(name_.empty()) {
        name_ = "UNKNOW";
    }
}

Thread::Thread(Thread&& other) noexcept :
    id_(other.id_),
    thread_(other.thread_),
    cb_(std::move(other.cb_)),
    name_(std::move(other.name_)) {
    other.id_ = -1;
    other.thread_ = 0;
}

Thread& Thread::operator=(Thread&& other) noexcept {
    if (this != &other) {
        id_ = other.id_;
        thread_ = other.thread_;
        cb_ = std::move(other.cb_);
        name_ = std::move(other.name_);
        other.id_ = -1;
        other.thread_ = 0;    
    }

    return *this;
}


Thread::~Thread() {
    if(thread_) {
        pthread_detach(thread_);
    }
}

void Thread::start() {
    NEMO_ASSERT(thread_ == 0);
    int ret = pthread_create(&thread_, nullptr, &Thread::Run, this);
    if(ret) {
        NEMO_LOG_ERROR(systemLogger) << "pthread_create thread fail, return=" 
                                 << ret
                                 << " name=" 
                                 << name_;
        throw std::logic_error("pthread_create error");
    }
}

void Thread::join() {
    if(thread_) {
        int ret = pthread_join(thread_, nullptr);
        if(ret) {
            NEMO_LOG_ERROR(systemLogger) << "pthread_join thread fail, return=" 
                                     << ret
                                     << " name=" 
                                     << thread_
                                     << " errno=" << errno
                                     << " errstr=" << strerror(errno);
            throw std::logic_error("pthread_join error");
        }
        thread_ = 0;
    } else {
        NEMO_ASSERT2(false, "join but thread not start");
    }
}

void Thread::detach() {
    if(thread_) {
        int ret = pthread_detach(thread_);
        if(ret) {
            NEMO_LOG_ERROR(systemLogger) << "pthread_detach thread fail, return=" 
                                     << ret
                                     << " name=" 
                                     << name_;
            throw std::logic_error("pthread_detach error");
        }
        thread_ = 0;
    } else {
        NEMO_ASSERT2(false, "detach but thread not start");
    }
}

void* Thread::Run(void* arg) {
    Thread* thread = reinterpret_cast<Thread*>(arg);
    currentThread = thread;
    currentThreadName = thread->name_;
    thread->id_ = nemo::GetCurrentThreadId();
    //pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).data()); //设置线程名称

    std::function<void()> cb(std::move(thread->cb_));

    try {
        cb();
    } catch (const std::exception& e) {
        NEMO_LOG_ERROR(systemLogger) << "thread except, "
            << " thread_id=" << thread->id_
            << " thread_name=" << thread->name_
            << " e.what()="
            << e.what();
        throw;
    } catch(...) {
        NEMO_LOG_ERROR(systemLogger) << "thread except, "
            << " thread_id=" << thread->id_
            << " thread_name=" << thread->name_;
        throw;
    }
    return thread;
}

} //namespace nemo