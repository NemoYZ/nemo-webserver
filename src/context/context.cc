#include "context/context.h"
#include "context/simple_stack_allocator.h"

namespace nemo {

static thread_local fcontext_t gContext;

typedef SimpleStackAllocator<8 * 1024 * 1024, Context::kStackSize, 8 * 1024> StackAllocator;

Context::Context(Fn fn, intptr_t vp, size_t stackSize) :
    fn_(fn),
    vp_(vp),
    stackSize_(stackSize) {
    stack_ = static_cast<char*>(StackAllocator::Allocate());
    ctx_ = make_fcontext(stack_, stackSize_, fn_);
}

Context::Context(Context&& other) noexcept :
    ctx_(other.ctx_),
    fn_(std::move(other.fn_)),
    vp_(other.vp_),
    stack_(other.stack_),
    stackSize_(other.stackSize_) {
    // other.ctx_ = nullptr;
    // other.vp_ = 0;
    other.stack_ = nullptr; //只需要把stack置空就能保证正确析构了
    // other.stackSize_ = 0;
}

Context::~Context() {
    if (stack_) {
        StackAllocator::Deallocate(stack_, stackSize_);
    }
}

Context& Context::operator=(Context&& other) noexcept {
    if (this != &other) {
        ctx_ = other.ctx_;
        fn_ = std::move(other.fn_);
        vp_ = other.vp_;
        stack_ = other.stack_;
        stackSize_ = other.stackSize_;
        // other.ctx_ = nullptr;
        // other.vp_ = 0;
        other.stack_ = nullptr;
        // other.stackSize_ = 0;
    }

    return *this;
}

void Context::SwapIn() {
    jump_fcontext(&gContext, ctx_, vp_);
}

void Context::SwapTo(Context& other) {
    jump_fcontext(ctx_, other.ctx_, other.vp_);
}

void Context::SwapOut() {
    jump_fcontext(ctx_, &gContext, 0);
}

} //namespace nemo