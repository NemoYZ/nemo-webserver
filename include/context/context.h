#pragma once

#include <memory>
#include "common/noncopyable.h"
#include "context/fcontext.h"

namespace nemo {

class Context : public Noncopyable {
public:
    typedef std::shared_ptr<Context> SharedPtr;
    typedef std::unique_ptr<Context> UniquePtr;
    typedef void (*Fn)(intptr_t);

public:
    constexpr static size_t kStackSize = 128 * 1024;

public:
    Context(Fn fn, intptr_t vp, size_t stackSize = kStackSize);
    Context(Context&& other) noexcept ;
    ~Context();
    Context& operator=(Context&& other) noexcept;

    void SwapIn();
    void SwapTo(Context& other);
    void SwapOut();

private:
    fcontext_t* ctx_;
    Fn fn_;
    uintptr_t vp_;
    char* stack_;
    size_t stackSize_;
};

}