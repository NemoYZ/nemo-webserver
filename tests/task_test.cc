#include <variant>

#include "coroutine/task.h"
#include "log/log.h"
#include "util/util.h"

using namespace nemo;

void TestFunc1();
void TestFunc2();
void Test();

static Logger::SharedPtr gRootLogger = NEMO_LOG_NAME("root");
static coroutine::Task::UniquePtr task1;
static coroutine::Task::UniquePtr task2;

int main(int argc, char** argv) {
    Test();
    for (int i = 0; i < 5; ++i) {
        task1->swapIn();
        task2->swapIn();
    }
    NEMO_LOG_DEBUG(gRootLogger) << "sizeof(Task::UniquePtr)=" << sizeof(coroutine::Task::UniquePtr);
    NEMO_LOG_DEBUG(gRootLogger) << "sizeof(Task::TaskFunction)=" << sizeof(coroutine::Task::Callback);
    NEMO_LOG_DEBUG(gRootLogger) << "sizeof(variant<Task::UniquePtr, Task::Callback>)=" 
        << sizeof(std::variant<coroutine::Task::UniquePtr, coroutine::Task::Callback>);
        
    return 0;
}

void TestFunc1() {
    for (int i = 0; i < 5;) {
        NEMO_LOG_DEBUG(gRootLogger) << "task1.id=" 
                << task1->getId() << " i = " << ++i;
        task1->swapOut();
    }
}

void TestFunc2() {
    for (int j = 0; j < 5;) {
        NEMO_LOG_DEBUG(gRootLogger) << "task2.id=" 
                << task2->getId() << " j = " << ++j;
        task2->swapOut();
    }
}

void Test() {
    task1.reset(new coroutine::Task(TestFunc1));
    task2.reset(new coroutine::Task(TestFunc2));
}