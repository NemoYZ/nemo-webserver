#pragma once

#include <assert.h>
#include <sys/param.h>

#include "util/util.h"
#include "log/log.h"

#if defined(__GNUC__)
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE inline
#endif //defined(__GNUC__)

#if defined(__GNUC__)
#define NEMO_LIKELY(x) __builtin_expect((x), 1)
#else
#define NEMO_LIKELY(x) (x)
#endif //defined(__GNUC__)

#if defined(__GNUC__)
#define NEMO_UNLIKELY(x) __builtin_expect((x), 0)
#else 
#define NEMO_UNLIKELY(x) (x)
#endif //defined(__GNUC__)

#define NEMO_ASSERT(expr) \
    if(NEMO_UNLIKELY(!(expr))) { \
        NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "ASSERTION: " #expr \
            << "\nbacktrace:\n" \
            << nemo::BacktraceToString(100, 2, "    "); \
        std::abort(); \
    }

#define NEMO_ASSERT2(expr, msg) \
    if(NEMO_UNLIKELY(!(expr))) { \
        NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "ASSERTION: " #expr \
            << " " \
            << msg \
            << "\nbacktrace:\n" \
            << nemo::BacktraceToString(100, 2, "    "); \
        std::abort(); \
    }

#define PAGE_SIZE EXEC_PAGESIZE