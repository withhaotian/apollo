/*
    常用工具函数
*/

#ifndef __APOLLO_UTIL_H__
#define __APOLLO_UTIL_H__

#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>

namespace apollo {
    // 获取线程id
    pid_t GetThreadId();

    // 获取协程id
    uint32_t GetFiberId();
}

#endif