#ifndef __APOLLO_MUTEX_H__
#define __APOLLO_MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

#include "noncopyable.h"

namespace apollo
{
// 信号量 
class Semaphore : Noncopyable
{
public:
    // 构造函数，count信号量大小
    Semaphore(uint32_t count = 0);

    ~Semaphore();

    // 获取信号量
    void wait();

    // 释放信号量
    void notify();

private:
    // 信号量
    sem_t m_semaphore;
};

// 局部锁模板类
// 可以视为mutex的代理，由此类的生命周期来自动管理mutex
template<class T>
class ScopedLockImpl {
public:
    // 思路是,在构造函数的时候自动上锁,在析构函数调用的时候自动解锁
    // 构造函数
    ScopedLockImpl(T& mutex) 
        : m_mutex(mutex) {
        m_mutex.lock();
        m_lock = true;
    }

    // 析构函数
    ~ScopedLockImpl() {
        unlock();
    }

    // 加锁
    void lock() {
        if(!m_lock) {
            m_mutex.lock();
            m_lock = true;
        }
    }

    // 解锁
    void unlock() {
        if(m_lock) {
            m_mutex.unlock();
            m_lock = false;
        }
    }
private:
    // 互斥量
    T& m_mutex;
    // 是否已上锁
    bool m_lock = false;
};

// 局部读锁
template<class T>
class ReadScopedLockImpl {
public:
    // 思路是,在构造函数的时候自动上锁,在析构函数调用的时候自动解锁
    // 构造函数
    ReadScopedLockImpl(T& mutex) 
        : m_mutex(mutex) {
        m_mutex.rdlock();
        m_lock = true;
    }

    // 析构函数
    ~ReadScopedLockImpl() {
        unlock();
    }

    // 加锁
    void lock() {
        if(!m_lock) {
            m_mutex.rdlock();
            m_lock = true;
        }
    }

    // 解锁
    void unlock() {
        if(m_lock) {
            m_mutex.unlock();
            m_lock = false;
        }
    }
private:
    // 互斥量
    T& m_mutex;
    // 是否已上锁
    bool m_lock = false;
};

// 局部写锁
template<class T>
class WriteScopedLockImpl {
public:
    // 思路是,在构造函数的时候自动上锁,在析构函数调用的时候自动解锁
    // 构造函数
    WriteScopedLockImpl(T& mutex) 
        : m_mutex(mutex) {
        m_mutex.wrlock();
        m_lock = true;
    }

    // 析构函数
    ~WriteScopedLockImpl() {
        unlock();
    }

    // 加锁
    void lock() {
        if(!m_lock) {
            m_mutex.wrlock();
            m_lock = true;
        }
    }

    // 解锁
    void unlock() {
        if(m_lock) {
            m_mutex.unlock();
            m_lock = false;
        }
    }
private:
    // 互斥量
    T& m_mutex;
    // 是否已上锁
    bool m_lock = false;
};

// 互斥量 
class Mutex: Noncopyable 
{
public:
    // 局部锁
    typedef ScopedLockImpl<Mutex> Lock;

    // 构造函数,初始化互斥量
    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    // 析构函数,销毁互斥量
    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    // 加锁
    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    // 解锁
    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    // linux线程互斥量
    pthread_mutex_t m_mutex;
};

// NULL互斥量(用于调试) 
class NullMutex: Noncopyable 
{
public:
    // 局部锁
    typedef ScopedLockImpl<NullMutex> Lock;

    // 构造函数,初始化互斥量
    NullMutex() {}

    // 析构函数,销毁互斥量
    ~NullMutex() {}

    // 加锁
    void lock() {}

    // 解锁
    void unlock() {}
private:
    // linux线程互斥量
    pthread_mutex_t m_mutex;
};

// 读写互斥量
class RWMutex : Noncopyable
{
public:
    // 局部读锁
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    // 局部写锁
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    // 构造函数,初始化互斥量
    RWMutex() {
        pthread_rwlock_init(&m_mutex, nullptr);
    }

    // 析构函数,销毁互斥量
    ~RWMutex() {
        pthread_rwlock_destroy(&m_mutex);
    }

    // 读加锁
    void rdlock() {
        pthread_rwlock_rdlock(&m_mutex);
    }

    // 写加锁
    void wrlock() {
        pthread_rwlock_wrlock(&m_mutex);
    }

    // 解锁
    void unlock() {
        pthread_rwlock_unlock(&m_mutex);
    }
private:
    // linux线程互斥量
    pthread_rwlock_t m_mutex;
};

// Null读写互斥量(用于调试)
class NullRWMutex : Noncopyable
{
public:
    // 局部读锁
    typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
    // 局部写锁
    typedef WriteScopedLockImpl<NullRWMutex> WriteLock;

    // 构造函数,初始化互斥量
    NullRWMutex() {}

    // 析构函数,销毁互斥量
    ~NullRWMutex() {}

    // 读加锁
    void rdlock() {}

    // 写加锁
    void wrlock() {}

    // 解锁
    void unlock() {}
private:
    // linux线程互斥量
    pthread_rwlock_t m_mutex;
};

// 自旋锁
class Spinlock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<Spinlock> Lock;

    // 构造函数
    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    // 析构函数
    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    // 上锁
    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    // 解锁
    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }
private:
    /// 自旋锁
    pthread_spinlock_t m_mutex;
};

// 原子锁
class CASLock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<CASLock> Lock;

    // 构造函数
    CASLock() {
        m_mutex.clear();
    }

    // 析构函数
    ~CASLock() {
    }

    // 上锁
    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    // 解锁
    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    /// 原子状态
    volatile std::atomic_flag m_mutex;
};


} // namespace apollo


#endif