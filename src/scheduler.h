#ifndef __APOLLO_SCHEDULER_H__
#define __APOLLO_SCHEDULER_H__

#include <memory>
#include <vector>

#include "fiber.h"
#include "thread.h"

namespace apollo
{
/*
    调度器的基本逻辑：
    [n:m模型
    n个线程调度m个协程
    每一时刻只有一个线程占用]
    1. 
*/
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    // 构造函数
    // threads：线程数量
    // use_caller：是否使用当前调用线程
    // name：调度器名称
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");

    virtual ~Scheduler();
    
    // 获取调度器名称
    const std::string& getName() const {return m_name;}

    // 返回当前调度器
    static Scheduler* GetThis();

    // 设置当前调度器
    static void SetThis(Scheduler* s);

    // 返回当前调度器主协程
    static apollo::Fiber* GetMainFiber();

    // 启动调度器
    void start();

    // 停止调度器
    void stop();

    // 获取任务队列任务数
    const int getNumTasks() const {return m_fibers.size();}

    // 协程调度
    // 默认-1即为不指定执行的线程
    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNolock(fc, thread);
        }
        if(need_tickle) {
            tickle();
        }
    }

    // 协程批量调度
    template<class InputIterator>
    void schedule(InputIterator begin, InputIterator end) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end) {
                need_tickle = scheduleNolock(&*begin, -1) || need_tickle;
                ++begin;
            }
        }
        if(need_tickle) {
            tickle();
        }
    }

// 子类可实现
protected:
    // 执行协程调度器
    void run();

    // 通知调度器有新任务
    virtual void tickle();

    // 判断是否可以停止
    virtual bool stopping();

    // 协程无任务可调度时切换回ide协程
    virtual void idle();

    // 查看是否还有空闲线程
    bool hasIdleThreads() const {return m_idleThreadCount > 0;}

private:
    // 协程调度启动(无锁)
    template<class FiberOrCb>
    bool scheduleNolock(FiberOrCb fc, int thread) {
        bool need_tickle = m_fibers.empty();
        Task tk(fc, thread);
        if(tk.fiber || tk.cb) {
            m_fibers.push_back(tk);
        }

        return need_tickle;
    }

private:
    // 协程/函数/线程-组
    struct Task {
        // 协程
        apollo::Fiber::ptr fiber;
        // 函数
        std::function<void()> cb;
        // 线程
        int thread;

        // 构造函数
        // 1)协程+线程
        Task(apollo::Fiber::ptr f, int thr) 
                : fiber(f)
                , thread(thr) {
        }
        Task(apollo::Fiber::ptr *f, int thr) 
                : fiber(std::move(*f))
                , thread(thr) {
        }

        // 2)协程执行函数+线程
        Task(std::function<void()> f, int thr) 
                : cb(f)
                , thread(thr) {
        }
        Task(std::function<void()> *f, int thr) 
                : cb(std::move(*f))
                , thread(thr) {
        }

        // 无参构造函数
        Task() : thread(-1) {
        }

        // 重置
        void reset() {
            fiber = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };

private:
    // 互斥量
    MutexType m_mutex;
    // 线程池
    std::vector<Thread::ptr> m_threads;
    // 协程队列
    std::list<Task> m_fibers;
    // 当use_caller为true时有效，主调度协程
    apollo::Fiber::ptr m_rootFiber;
    // 调度器名称
    std::string m_name;

protected:
    // 协程下的线程id数组
    std::vector<int> m_threadIds;
    // 线程数量
    size_t m_threadCount = 0;
    // 工作线程数量
    std::atomic<size_t> m_activeThreadCount = {0};
    // 空闲线程数量
    std::atomic<size_t> m_idleThreadCount = {0};
    // 是否正在停止
    bool m_stopping = true;
    // 是否自动停止
    bool m_autoStop = false;
    // 主线程id (use_caller)
    int m_rootThread = 0;
};

} // namespace apollo


#endif