#include <atomic>

#include "macro.h"
#include "fiber.h"
#include "log.h"
#include "config.h"
#include "scheduler.h"

namespace apollo
{
static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

apollo::ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    apollo::Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");

static std::atomic<uint64_t> s_fiber_id {0};                // 当前fiberid
static std::atomic<uint64_t> s_fiber_count {0};             // 总fiber数量

static thread_local Fiber* t_fiber = nullptr;               // 当前fiber
static thread_local Fiber::ptr t_mainFiber = nullptr;       // 主fiber

// 动态分配内存
class MallocStackAllocator {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }

    static void DeAlloc(void* vp, size_t size) {
        return free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

// 无参私有构造函数，实例化主fiber
Fiber::Fiber() {
    m_state = EXEC;
    SetThis(this);

    if(getcontext(&m_ctx)) {
        APOLLO_ASSERT2(false, "GETCONTEXT FAILED: ")
    }

    ++s_fiber_count;

    APOLLO_LOG_DEBUG(g_logger) << "MAIN FIBER CONSTRUCTION";
}

// 有参构造函数，实例化新fiber
Fiber::Fiber(std::function<void()> cb, size_t stackSize, bool use_caller)
    : m_id(++s_fiber_id)
    , m_cb(cb)
    , m_caller(use_caller) {
    ++s_fiber_count;

    m_stackSize = m_stackSize ? m_stackSize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stackSize);

    if(getcontext(&m_ctx)) {
        APOLLO_ASSERT2(false, "GETCONTEXT FAILED: ")
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stackSize;

    // 如果调用当前运行协程
    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;

    APOLLO_LOG_DEBUG(g_logger) << "FIBER ID: " << m_id << " CONSTRUCTED";
}

Fiber::~Fiber() {
    --s_fiber_count;

    if(m_stack) {   // 主协程不会有栈空间，因此当有栈空间默认为子协程
        APOLLO_ASSERT(m_state == TERM
                || m_state == INIT
                || m_state == EXCEPT);

        StackAllocator::DeAlloc(m_stack, m_stackSize);
    } else {    // 否则，析构主协程
        APOLLO_ASSERT(!m_cb);
        APOLLO_ASSERT(m_state == EXEC);

        Fiber* cur = t_fiber;
        if(cur == this) {
            SetThis(nullptr);
        }
    }
    APOLLO_LOG_DEBUG(g_logger) << "~FIBER ID: " << m_id
                            << " | TOTAL FIBERS: " << s_fiber_count;
}

// 重置协程执行函数，并设置状态
void Fiber::reset(std::function<void()> cb) {
    APOLLO_ASSERT(m_stack);
    APOLLO_ASSERT(m_state == TERM
                || m_state == EXCEPT
                || m_state == INIT);
    
    m_cb = cb;
    if(getcontext(&m_ctx)) {
        APOLLO_ASSERT2(false, "GETCONTEXT FAILED: ")
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stackSize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;     // 协程状态初始化为init
}

// 将当前协程切换到运行状态
void Fiber::swapIn() {
    SetThis(this);
    APOLLO_ASSERT(m_state != EXEC);
    m_state = EXEC;

    if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
        APOLLO_ASSERT2(false, "SWAPCONTEXT FAILED: ");
    }
}

// 将当前协程切换到后台
void Fiber::swapOut() {
    // 让出当前协程，将主协程作为活动协程
    SetThis(Scheduler::GetMainFiber());
    
    if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {
        APOLLO_ASSERT2(false, "SWAPCONTEXT FAILED: ");
    }
}

// 将当前协程切换到运行状态【由当前线程的主协程负责切换】
void Fiber::call() {
    SetThis(this);
    APOLLO_ASSERT(m_state != EXEC);
    m_state = EXEC;

    if(swapcontext(&t_mainFiber->m_ctx, &m_ctx)) {
        APOLLO_ASSERT2(false, "SWAPCONTEXT FAILED: ");
    }
}

// 将当前协程切换到后台【由当前线程的主协程负责切换】
void Fiber::back() {
    // 让出当前协程，将主协程作为活动协程
    SetThis(t_mainFiber.get());
    
    if(swapcontext(&m_ctx, &t_mainFiber->m_ctx)) {
        APOLLO_ASSERT2(false, "SWAPCONTEXT FAILED: ");
    }
}

// 设置当前的运行协程
void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

// 返回当前的运行协程
Fiber::ptr Fiber::GetThis() {
    if(t_fiber) {
        return t_fiber->shared_from_this();
    }
    // 创建主协程
    Fiber::ptr mainFiber(new Fiber);
    APOLLO_ASSERT(mainFiber.get() == t_fiber);
    t_mainFiber = mainFiber;
    return t_fiber->shared_from_this();
}

// 协程切换到后台，并设置为HOLD状态
void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    APOLLO_ASSERT(cur->m_state == EXEC);
    cur->m_state = HOLD;
    cur->swapOut();
}

// 协程切换到后台，并设置为READY状态
void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    APOLLO_ASSERT(cur->m_state == EXEC);
    cur->m_state = READY;
    cur->swapOut();
}

// 返回总协程数量
uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}

// 协程运行函数
void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();         // 智能指针引用数 +1
    APOLLO_ASSERT(cur);

    bool use_caller = cur->m_caller;

    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    } catch(const std::exception& e) {
        cur->m_state = EXCEPT;
        APOLLO_LOG_ERROR(g_logger) << "FIBER EXCEPT " << e.what()
            << " FIBER ID: " << cur->getId() << std::endl
            << apollo::BacktraceToString();
    } catch(...) {
        cur->m_state = EXCEPT;
        APOLLO_LOG_ERROR(g_logger) << "FIBER EXCEPT "
            << " FIBER ID: " << cur->getId() << std::endl
            << apollo::BacktraceToString();
    }
    
    auto raw_ptr = cur.get();
    cur.reset();                    // 将智能指针cur的引用数清零（否则cur将永远不会释放）
    
    // 注意当use_caller为true的时候，如果还是用swapOut的话，只会mainFiber和自身循环切换，导致没法结束
    if(use_caller) {
        raw_ptr->back();                // 需要手动切换回主协程
    } else {
        raw_ptr->swapOut();             // 需要手动切换回主协程
    }

    // 不会回到此处，因此加入断言判断
    APOLLO_ASSERT2(false, "NEVER REACH HERE, FIBER ID: " + std::to_string(raw_ptr->getId()));
}

// 返回协程id
uint64_t Fiber::GetFiberId() {
    if(t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}

} // namespace apollo
