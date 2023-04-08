#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include "coroutine/processor.h"
#include "common/noncopyable.h"

namespace nemo {
namespace net {
namespace io {

class Reactor;

class ReactorElement : Noncopyable {
public:
    typedef std::shared_ptr<ReactorElement> SharedPtr;
    typedef std::unique_ptr<ReactorElement> UniquePtr;

public:
    struct Entry {
        Entry() = default;
        Entry(size_t idx, std::shared_ptr<short int[]>& evs, 
            const coroutine::Processor::SuspendEntry& s) :
            index(idx), 
            events(evs),
            suspendEntry(s) {
        }
        bool operator==(const Entry& other) const {
            return index == other.index &&
                    events == other.events && 
                    suspendEntry == other.suspendEntry;
        }

        size_t index;
        std::shared_ptr<short int[]> events;
        coroutine::Processor::SuspendEntry suspendEntry;
    };

    typedef std::vector<Entry> EntryVector;

public:
    explicit ReactorElement(int fd);
    bool add(Reactor* reactor, short int pollEvent, 
        const Entry& entry);
    void trigger(Reactor* reactor, short int pollEvent);

protected:
    void onClose();
    EntryVector& select(short int pollEvent);
    void trigger(short int revent, EntryVector& entries);
    void rollback(EntryVector & entries, const Entry& entry);
    void removeExpired(EntryVector& entryList);

protected:
    EntryVector in_;
    EntryVector out_;
    EntryVector inAndOut_;
    EntryVector error_;
    std::mutex mutex_;
    int fd_;
    short int event_;
};

} // namespace io
} // namespace net
} // namespace nemo
