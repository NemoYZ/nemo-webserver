#pragma once

#include <utility>
#include <type_traits>

namespace nemo {

template<typename T, typename... Args>
void construct(T* p, Args&&... args) {
    ::new(p) T(std::forward<Args>(args)...);
}

template<typename T>
void destory(T* p) {
    if constexpr(!std::is_trivially_destructible_v<T>) {
        p->~T();
    }
}

} // namespace 