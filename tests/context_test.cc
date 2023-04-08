#include "context/simple_stack_allocator.h"
#include "context/context.h"
#include "log/log.h"

using namespace nemo;

static Logger::SharedPtr gRootLogger = NEMO_LOG_NAME("root");
static Context::UniquePtr ctx1;
static Context::UniquePtr ctx2;

void TestFunc1(intptr_t);
void TestFunc2(intptr_t);

int main(int argc, char** argv) {
    ctx1 = std::make_unique<Context>(TestFunc1, 0);
    ctx2 = std::make_unique<Context>(TestFunc2, 0);
    ctx1->SwapIn();

    return 0;
}

void TestFunc1(intptr_t) {
    for (int i = 0; i < 5;) {
        NEMO_LOG_DEBUG(gRootLogger) << " i = " << ++i;
        ctx1->SwapTo(*ctx2.get());
    }
}

void TestFunc2(intptr_t) {
    for (int j = 0; j < 5;) {
        NEMO_LOG_DEBUG(gRootLogger) << " j = " << ++j;
        ctx2->SwapTo(*ctx1.get());
    }
}