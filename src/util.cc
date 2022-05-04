#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <unistd.h> 
#include "util.h"

namespace apollo {
    // 获取线程id
    pid_t GetThreadId() {
        return syscall(SYS_gettid);
    }

    // 获取协程id
    uint32_t GetFiberId() {
        return 0;
    }
}