#pragma once

#include <memory>

#include "common/noncopyable.h"

namespace nemo {

/**
 * @brief 单例模式封装类
 * @details T 类型
 */
template<typename T>
class Singleton : Noncopyable {
public:
    /**
     * @brief 返回单例引用
     */
    static T& GetInstance() {
        static T instance{Token()};
        return instance;
    }

    virtual ~Singleton() = default;

protected:
    struct Token {};
    Singleton() = default;
};

/**
 * @brief 单例模式封装类
 * @details T 类型
 */
template<typename T>
class SingletonPtr : Noncopyable {
public:
    /**
     * @brief 返回单例指针
     */
    static T* GetInstance() {
        static std::unique_ptr<T> instance = std::make_unique<T>(Token());
        return instance.get();
    }

    virtual ~SingletonPtr() = default;

protected:
    struct Token {};
    SingletonPtr() = default;
};

} //namespace nemo