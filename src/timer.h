#ifndef __APOLLO_TIMER_H__
#define __APOLLO_TIMER_H__

#include <memory>
#include <set>
#include <vector>

#include "mutex.h"

namespace apollo
{
class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
    typedef std::shared_ptr<Timer> ptr;

    // 取消定时器
    bool cancel();

    // 刷新设置定时器的执行时间
    bool refresh();

    // 重置定时器时间
    bool reset(uint64_t ms, bool from_now);

private:
    // 私有构造函数,只能通过timermanager来构造timer
    Timer(uint64_t ms, std::function<void()> cb, 
        bool recurring, TimerManager* manager);
    Timer(uint64_t next);
private:
    // 是否循环定时器
    bool m_recurring = false;
    // 执行周期
    uint64_t m_ms = 0;
    // 精确的执行时间（绝对时间）
    uint64_t m_next = 0;
    // 回调函数
    std::function<void()> m_cb;
    // 定时器管理器
    TimerManager* m_mgr = nullptr;
private:
    // 用于set的比较函数
    // 思想是基于最小堆实现的timer
    struct Comparable {
        bool operator () (const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

class TimerManager {
friend class Timer;
public:
    typedef RWMutex RWMutexType;

    TimerManager();

    // 虚析构函数，本类将由IOmanager类继承
    virtual ~TimerManager();

    // 添加定时器
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, 
            bool recurring = false);

    // 添加条件定时器
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, 
            std::weak_ptr<void> weak_cond,
            bool recurring = false);

    //  获取最近的一个定时器时间
    uint64_t getNextTimer();

    // 获取需要执行的回调函数列表，即拿出所有超时的定时器并全部执行
    /** 
     * （发生的一个bug是没加&，所以需要执行的cb没有传出去，外面依然是空，
     * 并且循环定时器为true时，会一直重复m_timers中加入） 
    **/
    void listExpiredCbs(std::vector<std::function<void()>>& cbs);

    // 是否有定时器
    bool hasTimer();

protected:
    // （纯虚函数）当有新的定时器插入到了列表首部，需要通知调度器
    virtual void onTimerInsertAtFront() = 0;

    // 将定时器添加到管理器中
    void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);
private:
    RWMutexType m_mutex;
    // 定时器集合
    std::set<Timer::ptr, Timer::Comparable> m_timers;
    // 是否触发onTimerInsertedAtFront
    bool m_tickled = false;
    // 上一次的执行时间
    uint64_t m_previousTime = 0;
};
} // namespace apollo



#endif