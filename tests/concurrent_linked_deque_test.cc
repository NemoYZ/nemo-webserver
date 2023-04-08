#include "container/concurrent_linked_deque.h"

#include <vector>
#include <iostream>

#include "log/log.h"
#include "common/types.h"
#include "common/lexical_cast.h"
#include "common/thread.h"
#include "common/macro.h"

using namespace nemo;

static Logger::SharedPtr gRootLogger = NEMO_LOG_NAME("root");
static ConcurrentLinkedDeque<String> gQueue;

void PushFunc();
void PopFunc();
void TestPushAndPop();
void TestLink();
void TestPushAndPopBulk();

int main(int argc, char** argv) {
    //TestLink();
    TestPushAndPopBulk();

    return 0;
}

void PushFunc() {
    for (int i = 0; i < 10; ++i) {
        gQueue.pushBack(Thread::GetCurrentThreadName() + " " + LexicalCast<String>(i));
    }
}

void PopFunc() {
    String val;
    for (int i = 0; i < 10; ++i) {
        if (!gQueue.popFront(val)) {
            NEMO_LOG_DEBUG(gRootLogger) << "gQueue empty";
        } else {
            NEMO_LOG_DEBUG(gRootLogger) << "val=" << val;
        }
        sleep(1);
    }
}

void TestPushAndPop() {
    std::vector<Thread::UniquePtr> pushThreads;
    std::vector<Thread::UniquePtr> popThreads;
    for (int i = 0; i < 5; ++i) {
        pushThreads.emplace_back(new Thread(PushFunc, "push_thread" + LexicalCast<String>(i)));
        popThreads.emplace_back(new Thread(PopFunc, "pop_thread" + LexicalCast<String>(i)));
    }

    for (int i = 0; i < 5; ++i) {
        pushThreads[i]->start();
    }

    for (int i = 0; i < 5; ++i) {
        pushThreads[i]->join();
    }

    for (int i = 0; i < 5; ++i) {
        popThreads[i]->start();
    }

    for (int i = 0; i < 5; ++i) {
        popThreads[i]->join();
    }

    NEMO_LOG_DEBUG(gRootLogger) << "queue.isEmpty()=" << gQueue.isEmpty();
}

void TestLink() {
    gQueue.pushBack("0");
    gQueue.pushBack("1");
    gQueue.pushBack("2");
    gQueue.pushBack("3");
    ConcurrentLinkedDeque<String> queue2;
    queue2.pushBack("4");
    queue2.pushBack("5");
    queue2.pushBack("6");
    queue2.pushBack("7");
    //gQueue.pushBack(std::move(queue2));
    gQueue.pushFront(std::move(queue2));
    while (!gQueue.isEmptyUnsafe()) {
        String val;
        NEMO_ASSERT(gQueue.popFront(val));
        NEMO_LOG_DEBUG(gRootLogger) << val;
    }
}

void TestPushAndPopBulk() {
    ConcurrentLinkedDeque<int> queue;
    queue.pushBack(1);
    queue.pushBack(2);
    queue.pushBack(3);
    queue.pushBack(4);
    queue.pushBack(5);
    queue.pushBack(6);
    queue.pushBack(7);
    queue.pushBack(8);

    ConcurrentLinkedDeque<int> backQue = queue.popBackBulk(0);
    ConcurrentLinkedDeque<int> frontQue = queue.popFrontBulk(0);
    int num = 0;

    std::cout << "backQue:" << std::endl;
    while (!backQue.isEmptyUnsafe()) {
        backQue.popFront(num);
        std::cout << num << " ";
    }
    std::cout << std::endl;

    std::cout << "frontQue:" << std::endl;
    while (!frontQue.isEmptyUnsafe()) {
        frontQue.popFront(num);
        std::cout << num << " ";
    }
    std::cout << std::endl;
}