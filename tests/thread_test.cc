#include <unistd.h>

#include <vector>
#include <thread>
#include <mutex>

#include "log/log.h"
#include "common/thread.h"
#include "util/util.h"

using namespace nemo;

static Logger::SharedPtr gRootLogger = NEMO_LOG_ROOT();

static int count = 0;
static std::mutex gMutex;

void func1() {
    NEMO_LOG_INFO(gRootLogger) << "name: " 
                            << Thread::GetCurrentThreadName()
                            << " this.name: " 
                            << Thread::GetCurrentThread()->getName()
                            << " id: " 
                            << GetCurrentThreadId()
                            << " this.id: " 
                            << Thread::GetCurrentThread()->getId();
    for (int i = 0; i < 100000; ++i) {
        std::lock_guard<std::mutex> lockGuard(gMutex);
        ++count;
    }
}

void func2() {
    while(true) {
        NEMO_LOG_INFO(gRootLogger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
        sleep(1);
    }
}

void func3() {
     while(true) {
        NEMO_LOG_INFO(gRootLogger) << "========================================";
        sleep(1);
     }
}

void Test1();

class Foo {
public:
    void foo() {
        while (true) {
            NEMO_LOG_DEBUG(gRootLogger) << "this=" << this << "' foo";
            sleep(1);
        }
    }
private:
};

void TestMember();

int main(int argc, char* argv[]) {
    TestMember();

    return 0;
}

void Test1() {
    NEMO_LOG_INFO(gRootLogger) << "thread test begin";
    std::vector<Thread::UniquePtr> threads;
    for (int i = 0; i < 2; ++i) {    
        Thread::UniquePtr thread1(new Thread(&func1, "name" 
                    + std::to_string(i * 2)));
        Thread::UniquePtr thread2(new Thread(&func1, "name" 
                    + std::to_string(i * 2 + 1)));
        threads.push_back(std::move(thread1));
        threads.back()->start();
        threads.push_back(std::move(thread2));
        threads.back()->start();
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i]->join();
    }

    NEMO_LOG_DEBUG(gRootLogger) << "count=" << count;
    NEMO_LOG_INFO(gRootLogger) << "Thread test end";
}

void TestMember() {
    Foo foo;
    Thread thread(std::bind(&Foo::foo, &foo), "test thread");
    thread.start();
    thread.join();
}