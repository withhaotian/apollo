#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace apollo
{
static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;                 // 调度器局部变量
static thread_local Fiber* t_scheduler_fiber = nullptr;               // fiber局部变量

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) 
        : m_name(name) {
    // 需要断言给定的线程数量大于0
    APOLLO_ASSERT(threads > 0);

    // 如果use_caller，则使用当前线程
    if(use_caller) {
        // 通过GetThis实例化主协程（若不存在）
        apollo::Fiber::GetThis();
        -- threads;         // 忽略当前调用线程
        
        APOLLO_ASSERT(GetThis() == nullptr);        // 断言有存在调度器
        t_scheduler = this;
        
        // 将scheduler::run bind到协程函数
        m_rootFiber.reset(new apollo::Fiber(std::bind(&Scheduler::run, this), 0, true));
        // 将线程名称设置为当前调度器名称
        apollo::Thread::SetName(m_name);
        
        // 更新线程信息
        t_scheduler_fiber = m_rootFiber.get();
        m_rootThread = apollo::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    } else {
        m_rootThread = -1;
    }

    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    APOLLO_ASSERT(m_stopping);

    if(GetThis() == this) {
        t_scheduler = nullptr;
    }
}

// 返回当前调度器
Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}

// 设置当前调度器
void Scheduler::SetThis(Scheduler* s) {
    t_scheduler = s;
}

// 返回当前调度器主协程
Fiber* Scheduler::GetMainFiber() {
    return t_scheduler_fiber;
}

// 启动调度器
void Scheduler::start() {
    MutexType::Lock lock(m_mutex);
    // 如果调度器不处于停止状态，则不继续
    if(!m_stopping) {
        return;
    }
    m_stopping = false;

    // 线程池中没有元素
    APOLLO_ASSERT(m_threads.empty());
    
    m_threads.resize(m_threadCount);
    // 线程加入线程池中，并且将id加入线程id数组中
    for(size_t i = 0; i <  m_threadCount; i++) {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), 
                    m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }

    lock.unlock();
}

// 停止调度器
void Scheduler::stop() {
    // 自动停止设置为true
    m_autoStop = true;

    APOLLO_LOG_DEBUG(g_logger) << "~IOManager, execute Scheduler::stop() ---";

    // 如果存在主协程，并且线程数为0，主协程状态为TERM/INIT，则停止
    if(m_rootFiber && m_threadCount == 0
        && (m_rootFiber->getState() == apollo::Fiber::INIT
        || m_rootFiber->getState() == apollo::Fiber::TERM)) {
        APOLLO_LOG_INFO(g_logger) << this << " SCHEDULER STOPED";

        m_stopping = true;

        // TODO，有具体业务子类实现
        if(stopping()) {
            return;
        }
    }

    if(m_rootThread != -1) {    //  是否指定线程，否则默认为当前调度器
        APOLLO_ASSERT(GetThis() == this);
    } else {
        APOLLO_ASSERT(GetThis() != this);
    }

    m_stopping = true;

    for(size_t i = 0; i < m_threadCount; i++) {
        tickle();
    }
    
    // if use_caller, 则当前协程执行的线程不包含在m_threadCount中
    if(m_rootFiber) {
        tickle();
        if(!stopping()) {
            m_rootFiber->call();
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    // 释放剩余所有线程
    for(size_t i = 0; i < thrs.size(); i++) {
        thrs[i]->join();
    }
}

// 执行调度器
void Scheduler::run() {
    APOLLO_LOG_DEBUG(g_logger) << "SCHEDULER: " << m_name << " RUNNING";
    set_hook_enable(true);  // 将hook设置为true，实现异步
    // 切换为当前调度器
    SetThis(this);
    
    // 如果当前执行线程不是调度器的主线程(user_caller = false)
    if(apollo::GetThreadId() != m_rootThread) {
        t_scheduler_fiber = apollo::Fiber::GetThis().get();
    }

    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::ptr cb_fiber;

    Task tk;

    while(true) {
        // 初始化task
        tk.reset();

        bool tickle_me = false;
        bool is_active = false;     // 是否有活跃任务
        // 新增同步锁作用域
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_fibers.begin();
            while(it != m_fibers.end()) {
                // 如果线程号及不等于-1， 又不等于当前活动线程号
                // 说明该任务由其他特定线程执行
                if(it->thread != -1 && it->thread != apollo::GetThreadId()) {
                    ++ it;
                    tickle_me = true;
                    continue;
                }

                // 断言fiber或执行函数
                APOLLO_ASSERT(it->fiber || it->cb);
                
                // 如果当前协程处于正在执行中，则跳过
                if(it->fiber && it->fiber->getState() == apollo::Fiber::EXEC) {
                    ++ it;
                    continue;
                }

                tk = *it;
                is_active = true;
                ++ m_activeThreadCount;
                m_fibers.erase(it);
                break;
            }

            tickle_me |= (it != m_fibers.end());
        }

        if(tickle_me) {
            tickle();
        }

        // 进入处理任务的逻辑部分
        // 1. 当前为协程且状态合法
        // 【fix a bug here】什么状态为合法？——当前状态不为结束状态或者异常，就是处于一个合法状态
        if(tk.fiber && (tk.fiber->getState() != apollo::Fiber::EXCEPT
                        || tk.fiber->getState() != apollo::Fiber::TERM)) {
            // 执行当前协程
            tk.fiber->swapIn();     // 中断，执行协程任务
            --m_activeThreadCount;  // 中断结束，当前协程已经执行完成

            if(tk.fiber->getState() == apollo::Fiber::READY) {
                // 重新加入任务队列
                schedule(tk.fiber);
            } else if(tk.fiber->getState() != apollo::Fiber::TERM
                && tk.fiber->getState() != apollo::Fiber::EXCEPT) {
                // 既没有执行完成、ready、异常，则将当前协程挂起
                tk.fiber->m_state = apollo::Fiber::HOLD;
            }
            tk.reset();
        }
        // 2. 当前为协程执行函数
        else if(tk.cb) {
            // 如果cb_ciber对象已实例
            if(cb_fiber) {
                cb_fiber->reset(tk.cb);
            } else {    // 否则，new一个
                cb_fiber.reset(new Fiber(tk.cb));
            }

            tk.reset();

            // 执行协程函数
            cb_fiber->swapIn();
            --m_activeThreadCount;
            if(cb_fiber->getState() == apollo::Fiber::READY) {
                schedule(cb_fiber);
                cb_fiber.reset();
            } else if(cb_fiber->getState() != apollo::Fiber::TERM
                && cb_fiber->getState() != apollo::Fiber::EXCEPT) {
                cb_fiber->m_state = apollo::Fiber::HOLD;
                cb_fiber.reset();
            } else {
                cb_fiber->reset(nullptr);
            }
        }
        // 3. 执行idle协程作为缓冲，等待其他协程抢占
        else {
            if(is_active) {
                --m_activeThreadCount;
                continue;
            }

            if(idle_fiber->getState() == apollo::Fiber::TERM) {
                APOLLO_LOG_INFO(g_logger) << "IDLE FIBER TERMINATED";
                break;
            }

            ++m_idleThreadCount;
            idle_fiber->swapIn();
            --m_idleThreadCount;

            if(idle_fiber->getState() != apollo::Fiber::TERM
                && idle_fiber->getState() != apollo::Fiber::EXCEPT) {
                // 既没有执行完成、ready、异常，则将当前协程挂起
                idle_fiber->m_state = apollo::Fiber::HOLD;
            }
        }
    }
}

// 通知调度器有新任务
void Scheduler::tickle() {
    APOLLO_LOG_INFO(g_logger) << " TICKLE ";
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    // 自动停止为true，停止为true，任务队列为空，活动线程数为0
    return m_autoStop && m_stopping
        && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    APOLLO_LOG_INFO(g_logger) << " IDLE ";
    while(!stopping()) {
        apollo::Fiber::YieldToHold();
    }
}

} // namespace apollo
