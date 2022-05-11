#ifndef __APOLLO_THREAD_H__
#define __APOLLO_THREAD_H__

// c++11 - std::thread
// before c++11, pthread
// 业务场景主要以多线程读为主

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>

#include "noncopyable.h"
#include "mutex.h"

namespace apollo {
// 线程类
class Thread : Noncopyable { // 禁止类拷贝
public:
    typedef std::shared_ptr<Thread> ptr;

    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    // 获取线程id
    pid_t getId() const {return m_id;}

    // 获取线程名称
    std::string getName() const {return m_name;}

    // 等待线程执行完成
    void join();

    // 获取当前的线程指针
    static Thread* GetThis();

    // 针对日志模块，获取线程名称
    static const std::string GetName();

    // 设置线程名称
    static void SetName(const std::string& name);

private:
    // 线程运行
    static void* run(void* arg);

private:
    // 线程id
    pid_t m_id = -1;
    // 线程指针
    pthread_t m_thread = 0;
    // 线程执行函数
    std::function<void()>   m_cb;
    // 线程名
    std::string m_name;
    // 信号量
    Semaphore m_semaphore;
};

} // namespace apollo


#endif