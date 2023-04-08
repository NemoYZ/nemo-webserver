#include "coroutine/routine_sync_timer.h"
#include "coroutine/scheduler.h"
#include "log/log.h"

using namespace nemo;

static Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void TestFunc(const String& name);
void RunThread(int threadIdName);

int main(int argc, char** argcv) {
    int threadNameId{0};
    coroutine::RoutineSyncTimer* timer = coroutine::Scheduler::GetTimer();
    coroutine::RoutineSyncTimer::TimePoint nowTimePoint = coroutine::RoutineSyncTimer::Now();
    NEMO_LOG_DEBUG(rootLogger) << "now time point=" << nowTimePoint.time_since_epoch().count();
    timer->add(nowTimePoint + std::chrono::milliseconds(1000), std::bind(RunThread, ++threadNameId));
    timer->add(nowTimePoint + std::chrono::milliseconds(500), std::bind(RunThread, ++threadNameId));
    timer->add(nowTimePoint + std::chrono::milliseconds(1000), std::bind(RunThread, ++threadNameId));
    timer->add(std::chrono::milliseconds(1200), std::bind(RunThread, ++threadNameId));
    timer->add(std::chrono::milliseconds(5000), std::bind(RunThread, ++threadNameId));
    sleep(10);
    timer->add(nowTimePoint, std::bind(RunThread, ++threadNameId));
    sleep(5);
    //timer.stop();

    return 0;
}

void TestFunc(const String& name) {
    for (int i = 0; i < 5; ++i) {
        NEMO_LOG_DEBUG(rootLogger) << "name=" << name << " i=" << i;
    }
}

void RunThread(int threadIdName) {
    String threadName = LexicalCast<String>(threadIdName);
    Thread::UniquePtr thread(new Thread(std::bind(TestFunc, threadName), threadName));
    thread->start();
    thread->join();
}