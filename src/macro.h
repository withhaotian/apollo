#ifndef __APOLLO_MACRO_H__
#define __APOLLO_MACRO_H__

#include <assert.h>
#include <string.h>

#include "util.h"
#include "log.h"

#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#   define APOLLO_LIKELY(x)       __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#   define APOLLO_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define APOLLO_LIKELY(x)      (x)
#   define APOLLO_UNLIKELY(x)      (x)
#endif

/// 断言宏封装
#define APOLLO_ASSERT(x) \
    if(APOLLO_UNLIKELY(!(x))) { \
        APOLLO_LOG_ERROR(APOLLO_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << apollo::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }
    

/// 断言宏封装
#define APOLLO_ASSERT2(x, w) \
    if(APOLLO_UNLIKELY(!(x))) { \
        APOLLO_LOG_ERROR(APOLLO_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << apollo::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif