/**
 * Linux 创建线程函数(pthread.h)介绍
 * 1. int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
 *    功能说明：创建一个线程
 *    参数说明：# thread 这是一个输出值，给出的是初始化好的线程id
 *              typedef unsigned long int pthread_t;
 *             # attr 线程的属性，一般传NULL即可，一些取值如下
 *              没查到
 *              #  define __SIZEOF_PTHREAD_ATTR_T 56
 *              union pthread_attr_t {
 *                  char __size[__SIZEOF_PTHREAD_ATTR_T];
 *                  long int __align;
 *              };
 *              typedef union pthread_attr_t pthread_attr_t;
 *             # start_routine 线程执行的函数的指针
 *             # arg 线程执行的函数的参数
 *    返回说明：0 成功
 *             其他 失败
 * 2. int pthread_join(pthread_t thread, void **retval);
 *    功能说明：阻塞等待指定的线程结束
 *    参数说明：# thread 指定线程的线程id
 *             # retval 这是一个输出参数，当thread指定的线程被取消了，*retval就等于PTHREAD_CANCELED。
 *              如果多个线程同时join一个线程，结果就是未定义的(返回值为为ESRCH)。
 *    返回说明：0 成功
 *             其他 失败
 * 3. void pthread_exit(void *retval);
 *    功能说明：终止一个线程
 *    参数说明：# retval 函数的返回代码，如果pthread_join的第二个参数不为空，该值就赋给pthread_join
 *    返回说明：This function does not return to the caller.
 * 4. int pthread_detach(pthread_t thread);
 *    功能说明：detach一个线程。当一个detach的线程结束的时候，它的资源会被自动释放还给系统。
 *    参数说明：# thread 要detach的线程id
 *    返回说明：0 成功
 *             其他 失败
 * 5. pthread_t pthread_self(void);
 *    功能说明：得到调用该函数的线程的线程id，类似于getpid()
 *    参数说明：无
 *    返回说明：调用该函数的线程的id
 * 6. int pthread_setname_np(pthread_t thread, const char *name);
 *    功能说明：设置线程的名字(不多于16个字符, 包括'\0')
 *    参数说明：# thread 要设置名字的线程的id
 *             # name 线程名字
 *    返回说明：0 成功
 *             其他 失败
 * 7. int pthread_getname_np(pthread_t thread, const char *name, size_t len);
 *    功能说明：获得线程的名字
 *    参数说明：# thread 目标线程id
 *              # name 装线程名字的缓存
 *              # len 缓存大小
 *    返回说明：0 成功
 *             其他 失败
 */
#pragma once

#include <memory>
#include <functional>

#include "common/noncopyable.h"
#include "common/types.h"

namespace nemo {

/**
 * @brief 线程类
 */
class Thread : Noncopyable {
public:
    typedef std::shared_ptr<Thread> SharedPtr;  ///< 智能指针定义
    typedef std::unique_ptr<Thread> UniquePtr;  ///< 智能指针定义

    typedef std::function<void()> Callback;

    /**
     * @brief 构造函数
     * @param[in] cb 线程执行函数
     * @param[in] name 线程名称
     */
    Thread(Callback&& cb, StringArg name = "");

    Thread(const Callback& cb, StringArg name = "");

    Thread(Thread&& other) noexcept;

    Thread& operator=(Thread&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~Thread();
    
    /**
     * @brief 线程ID
     */
    pid_t getId() const { return id_; }

    /**
     * @brief 线程名称
     */
    const String& getName() const { return name_; }

    bool started() const { return thread_ != 0; }

    void start();

    /**
     * @brief 等待线程执行完成
     */
    void join();

    /**
     * @brief 
     */
    void detach();
    
    /**
     * @brief 获取当前执行的线程指针
     */
    static Thread* GetCurrentThread();

    /**
     * @brief 获取当前执行的线程名称
     */
    static const String& GetCurrentThreadName();

    /**
     * @brief 设置当前执行线程名称
     * @param[in] name 线程名称
     */
    static void SetCurrentThreadName(StringArg name);

    static long HardwareConcurrency();

    static size_t HashCode(const Thread& thread) { 
        return static_cast<size_t>(thread.id_); 
    }
    
    bool operator==(const Thread& other) const { return id_ == other.id_; }
    bool operator<(const Thread& other) const { return id_ < other.id_; }

private:
    /**
     * @brief 线程执行函数
     */
    static void* Run(void* arg);

private:
    pid_t id_ = -1;         ///< 线程id
    pthread_t thread_;      ///< 线程结构
    Callback cb_;           ///< 线程执行的函数
    String name_;           ///< 线程名称
};

}   //namespace Nemo
