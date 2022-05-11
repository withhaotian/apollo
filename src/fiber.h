#ifndef __APOLLO_FIBER_H__
#define __APOLLO_FIBER_H__

#include <memory>
#include <ucontext.h>
#include <functional>

namespace apollo
{

class Scheduler;

// 协程类-轻量级线程
class Fiber : public std::enable_shared_from_this<Fiber>
{
friend class Scheduler;
public:
    typedef std::shared_ptr<Fiber> ptr;

    // 定义协程状态
    enum State {
        // 初始化状态
        INIT,
        // 暂停状态
        HOLD,
        // 执行中状态
        EXEC,
        // 结束状态
        TERM,
        // 可执行状态
        READY,
        // 异常状态
        EXCEPT
    };

private:
    // 无参构造函数
    // 私有构造函数，a) 无法在外部实例化; b)  无法被继承
    Fiber();

public:
    // 构造函数
    // cb 协程执行函数
    // stackSize 协程运行栈大小
    Fiber(std::function<void()> cb, size_t stackSize = 0, bool use_caller = false);
    ~Fiber();
    
    // 重置协程执行函数，并设置状态
    void reset(std::function<void()> cb);

    // 将当前协程切换到后台
    void swapOut();

    // 将当前协程切换到运行状态
    void swapIn();

    // 将当前协程切换到运行状态【由当前线程的主协程负责切换】
    void call();

    // 将当前协程切换到后台【由当前线程的主协程负责切换】
    void back();

    // 获取协程id
    uint64_t getId() const {return m_id;}

    // 获取协程状态
    State getState() const {return m_state;};

public:
    // 设置当前的运行协程
    static void SetThis(Fiber* f);

    // 返回当前的运行协程
    static Fiber::ptr GetThis();

    // 协程切换到后台，并设置为HOLD状态
    static void YieldToHold();

    // 协程切换到后台，并设置为READY状态
    static void YieldToReady();

    // 返回总协程数量
    static uint64_t TotalFibers();

    // 协程运行函数
    static void MainFunc();

    // 返回协程id
    static uint64_t GetFiberId();

private:
    // 协程id
    uint64_t m_id = 0;
    // 运行栈大小
    uint32_t m_stackSize = 0;
    // 协程运行状态
    State m_state = INIT;
    // 协程上下文
    ucontext_t m_ctx;
    // 协程运行栈指针
    void* m_stack = nullptr;
    // 协程执行函数
    std::function<void()> m_cb;
    // 是否使用当前运行线程
    bool m_caller = false;
};

} // namespace apollo


#endif