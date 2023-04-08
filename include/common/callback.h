#pragma once

#include <memory>
#include <functional>
#include <utility>

namespace nemo {

/**
 * @brief 弱回调，可以判断对象是否已经被释放而是否调用
 * 
 * @tparam T 
 * @tparam Args 
 */
template<typename T, typename... Args>
class WeakCallback {
public:
    WeakCallback(const std::weak_ptr<T>& obj,
               const std::function<void(T*, Args...)>& f) : 
        object_(object), function_(function) {
    }

    // Default dtor, copy ctor and assignment are okay
    void operator()(Args&&... args) const {
        std::shared_ptr<T> ptr(obj_.lock());
        if (ptr) {
            func_(ptr.get(), std::forward<Args>(args)...);
        }
    }

private:
    std::weak_ptr<T> obj_;
    std::function<void(T*, Args...)> func_;
};

template<typename T, typename... Args>
inline WeakCallback<T, Args...> MakeWeakCallback(const std::shared_ptr<T>& object,
                                        void (T::*memberFunction)(Args...)) {
    return WeakCallback<T, Args...>(object, memberFunction);
}

template<typename T, typename... Args>
WeakCallback<T, Args...> MakeWeakCallback(const std::shared_ptr<T>& object,
                                        void (T::*memberFunction)(T...) const) {
    return WeakCallback<T, Args...>(object, memberFunction);
}

} //namespace nemo