#include "log/log.h"
#include "coroutine/scheduler.h"
#include "coroutine/processor.h"

using namespace nemo;

static Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");
static coroutine::Scheduler scheduler;
static coroutine::Processor processor(nullptr);

void ProcessTest();
void ProcessTest2();

int main(int argc, char** argv) {
    //ProcessTest();
    //ProcessTest2();
    
    return 0;
}

void ProcessTest() {
    Thread::UniquePtr thread(new Thread(std::bind(&coroutine::Processor::process, &processor), 
        "process test thread"));
    
    coroutine::Task::UniquePtr task(new coroutine::Task([](){
        for (int i = 0; i < 3; ++i) {
            NEMO_LOG_DEBUG(rootLogger) << i;
        }
        //throw std::runtime_error("task exception");
    }));
    processor.addTask(std::move(task));
    NEMO_LOG_DEBUG(rootLogger) << "here";
    processor.addTask([](){
        for (int i = 3; i < 6; ++i) {
            NEMO_LOG_DEBUG(rootLogger) << i;
        }
    });
    processor.addTask([](){
        for (int i = 6; i < 9; ++i) {
            NEMO_LOG_DEBUG(rootLogger) << i;
        }
    });
    thread->start();
    thread->join();
}

void ProcessTest2() {
    coroutine::Processor::UniquePtr processor2 = std::make_unique<coroutine::Processor>(nullptr);
    for (int i = 0; i < 10000; ++i) {
        processor2->addTask([](){
            NEMO_LOG_DEBUG(rootLogger) << "task_id=" 
                << coroutine::Processor::GetCurrentRunningTask()->getId();
        });
    }
    processor2->process();
}