#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif //!_GNU_SOURCE

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cxxabi.h>
#include <time.h>

#include <functional>
#include <vector>
#include <unordered_set>
#include <iterator>
#include <type_traits>
#include <utility>

#include <boost/lexical_cast.hpp>

#include "common/types.h"

namespace nemo {

/**
 * @brief 返回当前线程的ID
 */
pid_t GetCurrentThreadId();

/**
 * @brief 返回当前协程的ID
 */
uint64_t GetCurrentTaskId();

/**
 * @brief 获取当前的调用栈
 * @param[out] bt 保存调用栈
 * @param[in] size 最多返回层数
 * @param[in] skip 跳过栈顶的层数
 */
void Backtrace(std::vector<String>& bt, int size = 64, int skip = 1);

/**
 * @brief 获取当前栈信息的字符串
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
String BacktraceToString(int size = 64, int skip = 2, 
        StringArg prefix = "");

/**
 * @brief 获取当前时间戳(毫秒表示)
 */
uint64_t GetCurrentMillionSeconds();

uint32_t EncodeZigzag32(const int32_t& v);

uint64_t EncodeZigzag64(const int64_t& v);

int32_t DecodeZigzag32(const uint32_t& v);

int64_t DecodeZigzag64(const uint64_t& v);

template<typename T>
const char* Demangle() {
    static const char* typeName = nullptr;
#ifdef _MSC_VER
    typeName = std::typeid(T).name();
#elif defined(__GNUC__) || defined(__clang__)
    typeName = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
#else
    static_assert(false, "compiler not support");
#endif 
    return typeName;
}

template<typename V, typename MapType, typename K>
V GetParamValue(const MapType& map, const K& k, const V& defaultVal = V()) {
    auto iter = map.find(k);
    if(iter == map.end()) {
        return defaultVal;
    }
    try {
        return LexicalCast<V>(iter->second);
    } catch (const boost::bad_lexical_cast&) {
    }
    return defaultVal;
}

String Format(const char* format, ...);

String Format(const char* format, va_list ap);

String Time2Str(time_t ts, StringArg format);

time_t Str2Time(StringArg str, StringArg format);

void ListFiles(std::vector<String>& files, const String& path, 
        const std::unordered_set<String>& suffixes = {});

inline void MemoryZero(void* p, size_t n) {
    memset(p, 0, n);
}

template<typename T>
size_t Digit2Str(char* buf, T v) {
    static_assert(std::is_integral<T>::value, "integral required");
    static const char digits[] = "9876543210123456789";
    static_assert(sizeof(digits) == 20, "wrong number of digits");
    static const char* zero = digits + 9;
    
    T tmp = v;
    char* p = buf;

    do {
        int lsd = static_cast<int>(tmp % 10);
        tmp /= 10;
        *p++ = zero[lsd];
    } while (tmp != 0);

    if (v < 0) {
        *p++ = '-';
    }
    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

size_t Hex2Str(char* buf, uintptr_t v);

template<typename Fn, typename... Args>
auto InvokeSlowSystemCall(const Fn& f, Args&&... args) -> 
        decltype(f(std::forward<Args>(args)...)) {
    using ResultType = decltype(f(std::forward<Args>(args)...));
    ResultType result = -1;
    do {
        result = f(std::forward<Args>(args)...);
        if (-1 == result && EINTR == errno) {
            continue;
        }
        break;
    } while (true);
    
    return result;
}

}   //namespace nemo