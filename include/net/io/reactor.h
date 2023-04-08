#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <iostream>

#include "net/io/reactor_element.h"
#include "common/noncopyable.h"
#include "common/thread.h"

namespace nemo {
namespace net {
namespace io {

class Reactor : Noncopyable {
public:
    typedef std::shared_ptr<Reactor> SharedPtr;
    typedef std::unique_ptr<Reactor> UniquePtr;
    typedef ReactorElement::Entry Entry;

public:
    static int InitReactors(int n);
    static Reactor* Select(int fd) {
        static int dummy = InitReactors(1);
        static_cast<void>(dummy);
        return reactors_[fd % reactors_.size()].get();
    }
    static size_t ReactorCount() {
        return static_cast<size_t>(reactors_.size()); 
    }

public:
    Reactor();
    virtual ~Reactor();
    
    bool add(int fd, short int pollEvent, const Entry& entry);
    // ---------- call by element
    virtual bool addEvent(int fd, short int event, short int promiseEvent) = 0;
    virtual bool delEvent(int fd, short int event, short int promiseEvent) = 0;

protected:
    void start();
    void stop();
    virtual void run() = 0;

private:
    static std::vector<Reactor::UniquePtr> reactors_;

private:
    Thread::UniquePtr thread_;
    std::atomic<bool> started_;
};

} // namespace io
} // namespapce net
} // namespace nemo