#pragma once

#include <list>
#include <memory>
#include <mutex>

#include "common/noncopyable.h"

namespace nemo {

template<typename T>
class ConcurrentLinkedDeque : Noncopyable {
public:
    typedef std::shared_ptr<ConcurrentLinkedDeque<T>> SharedPtr;
    typedef std::unique_ptr<ConcurrentLinkedDeque<T>> UniquePtr;
    typedef typename std::list<T>::size_type size_type;
    typedef typename std::list<T>::iterator iterator;

public:
    iterator begin() { return queue_.begin(); }
    iterator end() { return queue_.end(); }

public:
    ConcurrentLinkedDeque() = default;
    template<std::input_iterator InputIter>
    ConcurrentLinkedDeque(InputIter first, InputIter last);
    ConcurrentLinkedDeque(ConcurrentLinkedDeque<T>&& other) noexcept;
    ConcurrentLinkedDeque(std::list<T>&& lst);
    ~ConcurrentLinkedDeque() = default;
    ConcurrentLinkedDeque<T>& operator=(ConcurrentLinkedDeque<T>&& other) noexcept;

    bool pushBackUnsafe(const T& val);
    bool pushBack(const T& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return pushBackUnsafe(val);
    }
    bool pushBackUnsafe(T&& val);
    bool pushBack(T&& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return pushBackUnsafe(std::move(val));
    }
    bool pushBackUnsafe(ConcurrentLinkedDeque<T>&& queue);
    bool pushBack(ConcurrentLinkedDeque<T>&& queue) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return pushBackUnsafe(std::move(queue));
    }
    template<typename... Args>
    bool emplaceBackUnsafe(Args&&... args);
    template<typename... Args>
    bool emplaceBack(Args&&... args) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return emplaceBackUnsafe(std::forward<Args>(args)...);
    }
    
    bool pushFrontUnsafe(const T& val);
    bool pushFront(const T& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return pushFrontUnsafe(val);
    }
    bool pushFrontUnsafe(T&& val);
    bool pushFront(T&& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return pushFrontUnsafe(std::move(val));
    }
    bool pushFrontUnsafe(ConcurrentLinkedDeque<T>&& queue);
    bool pushFront(ConcurrentLinkedDeque<T>&& queue) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return pushFrontUnsafe(std::move(queue));
    }
    template<typename... Args>
    bool emplaceFrontUnsafe(Args&&... args);
    template<typename... Args>
    bool emplaceFront(Args&&... args) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return emplaceFrontUnsafe(std::forward<Args>(args)...);
    }

    bool popFrontUnsafe(T& val);
    bool popFront(T& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return popFrontUnsafe(val);
    }
    bool popFrontUnsafe();
    bool popFront() {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return popFrontUnsafe();
    }
    ConcurrentLinkedDeque<T> popFrontBulkUnsafe(size_type n);
    ConcurrentLinkedDeque<T> popFrontBulk(size_type n);
    ConcurrentLinkedDeque<T> popAllUnsafe();
    ConcurrentLinkedDeque<T> popAll() {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return popAllUnsafe();
    }
    bool frontUnsafe(T& val);
    bool front(T& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return frontUnsafe(val);
    }

    bool popBackUnsafe(T& val);
    bool popBack(T& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return popBackUnsafe(val);
    }
    ConcurrentLinkedDeque<T> popBackBulkUnsafe(size_type n);
    ConcurrentLinkedDeque<T> popBackBulk(size_type n);
    bool popBackUnsafe();
    bool popBack() {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return popBackUnsafe();
    }
    bool backUnsafe(T& val);
    bool back(T& val) {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return backUnsafe(val);
    }

    size_type sizeUnsafe() const {
        return queue_.size();
    }
    size_type size() const {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return sizeUnsafe();
    }
    bool isEmptyUnsafe() {
        return queue_.empty();
    }
    bool isEmpty() {
        std::lock_guard<std::mutex> lockGuard(mutex_);
        return isEmptyUnsafe();
    }

    std::mutex& getMutext() { return mutex_; }

private:
    void innerPopFrontBulk(size_type n, std::list<T>& target);
    void innerPopBackBulk(size_type n, std::list<T>& target);

private:
    std::list<T> queue_;
    mutable std::mutex mutex_;
};

template<typename T>
void ConcurrentLinkedDeque<T>::innerPopFrontBulk(size_type n,
                                                std::list<T>& target) {
    using DiffType = typename std::list<T>::difference_type;

    const size_type queSize = queue_.size();
    // 注意，在早期的gcc中list的size的时间复杂度为O(n)
    // 若gcc版本较低，可改为一边移动迭代器，一边检查个数
    if (n >= queSize) {
        target = std::move(queue_);
    } else {
        size_type halfSize = queSize / 2; //queue_.size() >> 1
        typename std::list<T>::iterator iter;
        if (halfSize > n) {
            iter = queue_.begin();
            std::advance(iter, n);
            target.splice(target.begin(), queue_, queue_.begin(), iter);
        } else {
            iter = queue_.end();
            std::advance(iter, -static_cast<DiffType>(queSize - n));
        }
        target.splice(target.begin(), queue_, queue_.begin(), iter);
    }
}

template<typename T>
void ConcurrentLinkedDeque<T>::innerPopBackBulk(size_type n,
                                                std::list<T>& target) {
    using DiffType = typename std::list<T>::difference_type;

    size_type queSize = queue_.size();
    // 注意，在早期的gcc中list的size的时间复杂度为O(n)
    // 若gcc版本较低，可改为一边移动迭代器，一边检查个数
    if (n >= queSize) {   
        target = std::move(queue_);
    } else {
        size_type halfSize = queSize / 2; //queue_.size() >> 1
        typename std::list<T>::iterator iter;
        if (halfSize > n) {
            iter = queue_.end();
            std::advance(iter, -static_cast<DiffType>(n));
        } else {
            iter = queue_.begin();
            std::advance(iter, queSize - n);
        }
        target.splice(target.begin(), queue_, iter, queue_.end());
    }
}


template<typename T>
template<std::input_iterator InputIter>
ConcurrentLinkedDeque<T>::ConcurrentLinkedDeque(InputIter first, InputIter last) :
    queue_(first, last) {
}

template<typename T>
ConcurrentLinkedDeque<T>::ConcurrentLinkedDeque(ConcurrentLinkedDeque<T>&& other) noexcept :
    queue_(std::move(other.queue_)) {
}

template<typename T>
ConcurrentLinkedDeque<T>::ConcurrentLinkedDeque(std::list<T>&& lst) :
    queue_(std::move(lst)) {
}

template<typename T>
ConcurrentLinkedDeque<T>& 
ConcurrentLinkedDeque<T>::operator=(ConcurrentLinkedDeque<T>&& other) noexcept {
    if (this != &other) {
        queue_ = std::move(other.queue_);
    }

    return *this;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::pushBackUnsafe(T&& val) {
    queue_.push_back(std::move(val));
    return true;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::pushBackUnsafe(ConcurrentLinkedDeque<T>&& queue) {
    queue_.splice(queue_.end(), queue.queue_);
    return true;
}

template<typename T>
template<typename... Args>
bool ConcurrentLinkedDeque<T>::emplaceBackUnsafe(Args&&... args) {
    queue_.emplace_back(std::forward<Args>(args)...);
    return true;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::pushFrontUnsafe(const T& val) {
    queue_.push_front(val);
    return true;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::pushFrontUnsafe(T&& val) {
    queue_.push_front(std::move(val));
    return true;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::pushFrontUnsafe(ConcurrentLinkedDeque<T>&& queue) {
    queue_.splice(queue_.begin(), queue.queue_);
    return true;
}

template<typename T>
template<typename... Args>
bool ConcurrentLinkedDeque<T>::emplaceFrontUnsafe(Args&&... args) {
    queue_.emplace_front(std::forward<Args>(args)...);
    return true;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::popFrontUnsafe(T& val) {
    if (!queue_.empty()) {
        val = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }
    return false;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::popFrontUnsafe() {
    if (!queue_.empty()) {
        queue_.pop_front();
        return true;
    }
    return false;
}   

template<typename T>
ConcurrentLinkedDeque<T> 
ConcurrentLinkedDeque<T>::popFrontBulkUnsafe(size_type n) {
    ConcurrentLinkedDeque<T> result;
    innerPopFrontBulk(n, result.queue_);
    return result;
}

template<typename T>
ConcurrentLinkedDeque<T> 
ConcurrentLinkedDeque<T>::popFrontBulk(size_type n) {
    ConcurrentLinkedDeque<T> result;
    std::lock_guard<std::mutex> lockGuard(mutex_);
    innerPopFrontBulk(n, result.queue_);
    return result;
}

template<typename T>
ConcurrentLinkedDeque<T> ConcurrentLinkedDeque<T>::popAllUnsafe() {
    ConcurrentLinkedDeque<T> result;
    result.queue_.splice(result.queue_.begin(), queue_);
    return result;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::frontUnsafe(T& val) {
    if (!queue_.empty()) {
        val = queue_.front();
        return true;
    }
    return false;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::popBackUnsafe(T& val) {
    if (!queue_.empty()) {
        val = std::move(queue_.back());
        queue_.pop_back();
        return true;
    }
    return false;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::popBackUnsafe() {
    if (!queue_.empty()) {
        queue_.pop_back();
        return true;
    }
    return false;
}

template<typename T>
ConcurrentLinkedDeque<T>
ConcurrentLinkedDeque<T>::popBackBulkUnsafe(size_type n) {
    ConcurrentLinkedDeque<T> result;
    innerPopBackBulk(n, result.queue_);
    return result;
}

template<typename T>
ConcurrentLinkedDeque<T>
ConcurrentLinkedDeque<T>::popBackBulk(size_type n) {
    ConcurrentLinkedDeque<T> result;
    std::lock_guard<std::mutex> lockGuard(mutex_);
    innerPopBackBulk(n, result.queue_);
    return result;
}

template<typename T>
bool ConcurrentLinkedDeque<T>::backUnsafe(T& val) {
    if (!queue_.empty()) {
        val = queue_.back();
        return true;
    }
    return false;
}

} //namespace nemo