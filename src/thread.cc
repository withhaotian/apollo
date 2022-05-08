#include "thread.h"
#include "log.h"
#include "util.h"

namespace apollo {

static thread_local  Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";

// 初始化静态系统日志器，默认名为system
static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

// 获取当前的线程
Thread* Thread::GetThis() {
    return t_thread;
}

// 针对日志模块，获取线程名称
const std::string Thread::GetName() {
    return t_thread_name;
}

// 设置线程名称
void Thread::SetName(const std::string& name) {
    if(name.empty())    return;
    if(t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name) 
        : m_cb(cb)
        , m_name(name) {
    if(name.empty()) {
        m_name = "UNKOWN";
    }

    // 创建线程
    // pthread_create的返回值:若成功，返回0；若出错，返回出错编号。
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);

    if(rt) {
        APOLLO_LOG_ERROR(g_logger) << "PTHREAD CREATE THREAD FAIL, RETURN: "
            << rt << " THREAD NAME: " << m_name;
        throw std::logic_error("PTHREAD CREATE THREAD ERROR");
    }
    
}
Thread::~Thread() {
    if(m_thread) {
        pthread_detach(m_thread);   // 销毁线程
    }
}

void Thread::join() {
    if(m_thread) {
        // 线程加入
        int rt = pthread_join(m_thread, nullptr);
        if(rt) {
            APOLLO_LOG_ERROR(g_logger) << "PTHREAD JOIN FAIL, RETURN: "
                << rt << "THREAD NAME: " << m_name;
            throw std::logic_error("PTHREAD JOIN ERROR");
        }
        m_thread = 0;
    }
}

void* Thread::run(void* arg) {
    Thread* thread = (Thread*)arg;

    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = apollo::GetThreadId();
    // pthread_setname_np, pthread_getname_np ：设置/获取线程的名称
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;

    cb.swap(thread->m_cb);

    cb();

    return 0;
}

} // namespace apollo

