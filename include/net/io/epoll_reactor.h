#pragma once

#include <memory>

#include "net/io/reactor.h"

namespace nemo {
namespace net {
namespace io {

class EpollReactor : public Reactor {
public:
    typedef std::shared_ptr<EpollReactor> SharedPtr;
    typedef std::unique_ptr<EpollReactor> UniquePtr;

public:
    EpollReactor();

public:
    bool addEvent(int fd, short int event, short int promiseEvent) override;
    bool delEvent(int fd, short int event, short int promiseEvent) override;

protected:
    void run() override;

private:
    constexpr static int MAX_EVENTS = 1024;
    constexpr static int MAX_TIMEOUT = 10;

private:
    int epfd_;
};

} // namespace io
} // namespace net
} // namespace nemo