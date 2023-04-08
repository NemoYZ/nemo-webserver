#include "net/io/reactor_element.h"

#include <poll.h>

#include <algorithm>

#include "log/log.h"
#include "net/io/reactor.h"
#include "util/file_descriptor.h"
#include "common/macro.h"

namespace nemo {
namespace net {
namespace io {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

void ReactorElement::onClose() {
    trigger(nullptr, POLLNVAL);
}

ReactorElement::EntryVector& ReactorElement::select(short int pollEvent) {
    if ((pollEvent & POLLIN) && (pollEvent & POLLOUT)) {
        return inAndOut_;
    } else if (pollEvent & POLLIN) {
        return in_;
    } else if (pollEvent & POLLOUT) {
        return out_;
    }   

    return error_;
}

void ReactorElement::trigger(short int revent, EntryVector& entries) {
    for (Entry& entry : entries) {
        entry.events[entry.index] = revent;
        coroutine::Processor::WakeUp(entry.suspendEntry);
    }
    entries.clear();
}

void ReactorElement::rollback(EntryVector& entries, const Entry& entry) {
    auto iter = std::find(entries.begin(), entries.end(), entry);
    if (iter != entries.end()) {
        entries.erase(iter);
    }
}

void ReactorElement::removeExpired(EntryVector& entries) {
    auto expiredBegin = std::remove_if(entries.begin(), entries.end(), [](const Entry& entry) {
        return entry.suspendEntry.isExpired();
    });
    entries.erase(expiredBegin, entries.end());
}

ReactorElement::ReactorElement(int fd) :
    fd_(fd),
    event_(0) {
}

bool ReactorElement::add(Reactor* reactor, short int pollEvent, 
        const Entry& entry) {
    std::lock_guard<std::mutex> lockGard(mutex_);

    EntryVector& entryVector = select(pollEvent);
    removeExpired(entryVector);
    entryVector.emplace_back(entry);

    short int addEvent = pollEvent & (POLLIN | POLLOUT);
    if (0 == addEvent) {
        addEvent |= POLLERR;
    }

    short int promiseEvent = event_ | addEvent; //在原有的event上加新的event
    addEvent = promiseEvent & ~event_;          //去掉已经添加过的event, 只保留没添加过的event

    if (promiseEvent != event_) {
        if (!reactor->addEvent(fd_, addEvent, promiseEvent)) {
            // add error.
            rollback(entryVector, entry);
            return false;
        } else {
            event_ = promiseEvent;
        }
    }
    return true;
}

void ReactorElement::trigger(Reactor* reactor, short int pollEvent) {
    std::lock_guard<std::mutex> lockGuard(mutex_);

    short int errEvent = POLLERR | POLLHUP | POLLNVAL;
    short int promiseEvent = 0;

    // 触发读事件
    short int check = POLLIN | errEvent;
    if (pollEvent & check) {
        trigger(pollEvent & check, in_);
    } else if (!in_.empty()) {
        promiseEvent |= POLLIN;
    }

    // 触发写事件
    check = POLLOUT | errEvent;
    if (pollEvent & check) {
        trigger(pollEvent & check, out_);
    } else if (!out_.empty()) {
        promiseEvent |= POLLOUT;
    }

    // 触发读写事件
    check = POLLIN | POLLOUT | errEvent;
    if (pollEvent & check) {
        trigger(pollEvent & check, inAndOut_);
    } else if (!inAndOut_.empty()) {
        promiseEvent |= POLLIN | POLLOUT;
    }

    // 触发错误事件
    check = errEvent;
    if (pollEvent & check) {
        trigger(pollEvent & check, error_);
    } else if (!error_.empty()) {
        promiseEvent |= POLLERR;
    }

    // 删掉没有触发的事件
    short int delEvent = event_ & ~promiseEvent;
    if (promiseEvent != event_) {
        if (reactor && reactor->delEvent(fd_, delEvent, promiseEvent)) {
            event_ = promiseEvent;
        }
    }
}

} // namespace io
} // namespace net
} // namespace nemo