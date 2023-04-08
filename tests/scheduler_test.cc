#include "log/log.h"
#include "coroutine/scheduler.h"

using namespace nemo;

static Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");
static coroutine::Scheduler scheduler("global scheduler");

void TestFunc1();
void TestSchedule();

int main(int argc, char** argv) {
    
    // sleep(1);
    scheduler.addTask(TestFunc1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // TestSchedule();

    scheduler.start();

    return 0;
}

void TestFunc1() {
    for (int i = 0; i < 10; ++i) {
        NEMO_LOG_DEBUG(rootLogger) << "i=" << i;
        coroutine::Processor::Suspend(std::chrono::seconds(1));
        coroutine::Processor::Yield();
    }
}

void TestSchedule() {
    for (int i = 0; i < 100; ++i) {
        scheduler.addTask([](){
            NEMO_LOG_DEBUG(rootLogger) << "task_id=" 
                << coroutine::Processor::GetCurrentRunningTask()->getId();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}