#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <unistd.h> 
#include <sstream>
#include <time.h>
#include <execinfo.h>

#include "util.h"
#include "fiber.h"
#include "log.h"

namespace apollo {

static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

// 获取线程id
pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

// 获取协程id
uint32_t GetFiberId() {
    return apollo::Fiber::GetFiberId();
}

static std::string demangle(const char* str) {
    size_t size = 0;
    int status = 0;
    std::string rt;
    rt.resize(256);
    if(1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
        char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if(v) {
            std::string result(v);
            free(v);
            return result;
        }
    }
    if(1 == sscanf(str, "%255s", &rt[0])) {
        return rt;
    }
    return str;
}

// 获取当前的调用栈
void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s = ::backtrace(array, size);

    char** strings = backtrace_symbols(array, s);
    if(strings == NULL) {
        APOLLO_LOG_ERROR(g_logger) << "Backtrace_synbols error";
        return;
    }

    for(size_t i = skip; i < s; ++i) {
        bt.push_back(demangle(strings[i]));
    }

    free(strings);
    free(array);
}

// 获取当前栈信息的字符串
std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(size_t i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

// 获取当前时间
uint64_t GetCurrentMS() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

}