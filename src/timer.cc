#include "timer.h"
#include "util.h"
#include "log.h"

namespace apollo
{

static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

bool Timer::Comparable::operator () (const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    if(!lhs && !rhs)    return false;
    else if(!lhs)       return true;
    else if(!rhs)       return false;
    else if(lhs->m_next != rhs->m_next)
                        return lhs->m_next < rhs->m_next;
    else                return lhs.get() < rhs.get();   // 如果值相等，则比较地址大小
}

// 私有构造函数
Timer::Timer(uint64_t ms, std::function<void()> cb, 
        bool recurring, TimerManager* manager) 
        : m_recurring(recurring)
        , m_ms(ms)
        , m_cb(cb)
        , m_mgr(manager) {
    // ms传入的是相对时间，需要转换成绝对时间
    m_next = apollo::GetCurrentMS() + ms;
}

Timer::Timer(uint64_t next) 
        : m_next(next) {
}

// 取消定时器
bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_mgr->m_mutex);
    if(m_cb) {
        m_cb = nullptr;
        auto it = m_mgr->m_timers.find(shared_from_this());
        m_mgr->m_timers.erase(it);
        return true;
    }
    return false;
}

// 刷新设置定时器的执行时间
bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(m_mgr->m_mutex);
    if(!m_cb)   return false;

    auto it = m_mgr->m_timers.find(shared_from_this());
    // 没找到
    if(it == m_mgr->m_timers.end()) return false;

    m_mgr->m_timers.erase(it);
    m_next = apollo::GetCurrentMS() + m_ms;
    m_mgr->m_timers.insert(shared_from_this());
    return true;
}

// 重置定时器时间
bool Timer::reset(uint64_t ms, bool from_now) {
    // 如果时间没有变化，并且并非强制当前时间重置
    if(ms == m_ms && !from_now)   return true;
    
    TimerManager::RWMutexType::WriteLock lock(m_mgr->m_mutex);
    if(!m_cb)   return false;

    auto it = m_mgr->m_timers.find(shared_from_this());
    // 没找到
    if(it == m_mgr->m_timers.end()) return false;

    m_mgr->m_timers.erase(it);

    // 新的定时时间
    uint64_t start = 0;
    if(from_now) {
        // 强制更新为当前时间
        start = apollo::GetCurrentMS();
    } else {
        start = m_next - m_ms;
    }

    m_ms = ms;
    m_next = start + m_ms;
    m_mgr->addTimer(shared_from_this(), lock);
    return true;
}

// 构造函数
TimerManager::TimerManager() {
    // 获取上一次执行的时间
    m_previousTime = apollo::GetCurrentMS();
}

// 虚析构函数，本类将由IOmanager类继承
TimerManager::~TimerManager() {
}

// 添加定时器
Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, 
        bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));

    RWMutexType::WriteLock lock(m_mutex);
    addTimer(timer, lock);

    return timer;
}
// 将定时器添加到管理器中
void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
    auto it = m_timers.insert(val).first;
    bool atFront = (it == m_timers.begin()) && !m_tickled;

    if(atFront) {
        m_tickled = true;
    }

    lock.unlock();

    if(atFront) {
        onTimerInsertAtFront();
    }
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    // weak_ptr.lock()获取shared_ptr
    std::shared_ptr<void> tmp = weak_cond.lock();
    // 只有当条件依然存在的时候，才执行cb()
    if(tmp) {
        cb();
    }
}

// 添加条件定时器
Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, 
        std::weak_ptr<void> weak_cond,
        bool recurring) {
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring); // 绑定Ontimer函数
}

//  获取最近的一个定时器时间
uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock lock(m_mutex);

    m_tickled = false;
    
    if(m_timers.empty()) {
        return ~0ull;   // 定时器为空，返回最大值
    }

    const Timer::ptr& it = *m_timers.begin();
    uint64_t now_ms = apollo::GetCurrentMS();
    // 说明最近的定时器已经超时了
    if(it->m_next <= now_ms) {
        return 0;
    } else {
        return it->m_next - now_ms;
    }
}

// 获取需要执行的回调函数列表，即拿出所有超时的定时器并全部执行
/** 
 * （发生的一个bug是没加&，所以需要执行的cb没有传出去，外面依然是空，
 * 并且循环定时器为true时，会一直重复m_timers中加入） 
    **/
void TimerManager::listExpiredCbs(std::vector<std::function<void()>>& cbs) {
    uint64_t now_ms = apollo::GetCurrentMS();
    
    // APOLLO_LOG_INFO(g_logger) << "m_timers.size() = " << m_timers.size();

    std::vector<Timer::ptr> expired;
    {   
        // 读锁
        RWMutexType::ReadLock lock(m_mutex);
        if(m_timers.empty()) {
            return;
        }
    }

    // 写锁
    RWMutexType::WriteLock lock(m_mutex);
    if(m_timers.empty()) {
        return;
    }

    // 没有超时的定时器
    if((*m_timers.begin())->m_next > now_ms) {
        return;
    }

    Timer::ptr nowTimer(new Timer(now_ms));

    auto it = m_timers.upper_bound(nowTimer);
    
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);

    cbs.reserve(expired.size());

    for(auto& it : expired) {
        cbs.push_back(it->m_cb);
        // 如果循环定时，需要重新加入定时器列表中
        if(it->m_recurring) {
            it->m_next = now_ms + it->m_ms;
            m_timers.insert(it);
        } else {
            it->m_cb = nullptr;
        }
    }
}

bool TimerManager::hasTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

} // namespace apollo
