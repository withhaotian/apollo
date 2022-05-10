/*
    常用工具函数
*/

#ifndef __APOLLO_UTIL_H__
#define __APOLLO_UTIL_H__

#include <sys/syscall.h>
#include <pthread.h>
#include <string>
#include <cxxabi.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>

namespace apollo {
// 获取线程id
pid_t GetThreadId();

// 获取协程id
uint32_t GetFiberId();

// 获取当前的调用栈
// bt 保存调用栈
// size 最多返回层数
// skip 跳过栈顶的层数
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);


// 获取当前栈信息的字符串
// size 栈的最大层数
// skip 跳过栈顶的层数
// prefix 前缀输出内容
std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");
}

#endif