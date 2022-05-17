#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

namespace apollo {

// 系统日志
static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

enum EpollCtlOp {
};

// 获取当前IOManager
IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

// 构造函数
IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    : Scheduler(threads, use_caller, name) {
    m_epfd = epoll_create(5000);    // 创建epoll接口
    APOLLO_ASSERT(m_epfd > 0);      // success 返回非负数
    
    // 利用pipeline双向传输消息，在epoll上层封装，实现异步多路复用IO
    int rt = pipe(m_tickleFds);
    APOLLO_ASSERT(!rt);

    // 定义epoll事件
    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    // fcntl()  performs  one of the operations described 
    // below on the open file descriptor fd.  The operation is de‐
    // termined by cmd.
    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);        // 设置非阻塞
    APOLLO_ASSERT(!rt);

    // epoll的事件注册函数，它不同与select()是在监听事件时告诉内核要监听什么类型的事件，
    // 而是在这里先注册要监听的事件类型。
    // 第一个参数是epoll_create()的返回值，第二个参数表示动作，用三个宏来表示：
    // EPOLL_CTL_ADD：注册新的fd到epfd中；
    // EPOLL_CTL_MOD：修改已经注册的fd的监听事件；
    // EPOLL_CTL_DEL：从epfd中删除一个fd；
    // 第三个参数是需要监听的fd，第四个参数是告诉内核需要监听什么事件
    rt  = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    APOLLO_ASSERT(!rt);

    resizeContext(32);

    start();    // 启动调度器
}

IOManager::~IOManager() {
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    // 释放申请的空间
    for(size_t i = 0; i < m_fdContexts.size(); i++) {
        if(m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

// 添加事件，成功返回0，失败返回-1
int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    FdContext* fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    
    if((int)m_fdContexts.size() > fd) {
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } 
    // 如果要添加的事件句柄不在事件上下文容器中，则扩容，并添加
    else {
        lock.unlock();
        // 加入写锁
        RWMutexType::WriteLock lock2(m_mutex);
        resizeContext(fd * 1.5);          // 扩容
        fd_ctx = m_fdContexts[fd];
    }
    
    FdContext::MutexType::Lock lock3(fd_ctx->mutex);
    if(APOLLO_UNLIKELY(fd_ctx->events & event)) {
        APOLLO_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                    << " event=" << (EPOLL_EVENTS)event
                    << " fd_ctx.event=" << (EPOLL_EVENTS)fd_ctx->events;
        APOLLO_ASSERT(!(fd_ctx->events & event));
    }

    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;

    // 事件注册
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        APOLLO_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events="
            << (EPOLL_EVENTS)fd_ctx->events;
        return -1;
    }

    ++m_pendingEventCount;
    fd_ctx->events = (Event)(fd_ctx->events | event);
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    APOLLO_ASSERT(!event_ctx.scheduler
                && !event_ctx.fiber
                && !event_ctx.cb);

    event_ctx.scheduler = Scheduler::GetThis();
    if(cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.fiber = Fiber::GetThis();
        APOLLO_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC
                    ,"state=" << event_ctx.fiber->getState());
    }
    return 0;
}

// 删除事件
bool IOManager::delEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(APOLLO_UNLIKELY(!(fd_ctx->events & event))) {
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        APOLLO_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);
    return true;
}

// 取消事件
bool IOManager::cancelEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(APOLLO_UNLIKELY(!(fd_ctx->events & event))) {
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        APOLLO_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

// 取消所有事件
bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events) {
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        APOLLO_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    if(fd_ctx->events & READ) {
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }
    if(fd_ctx->events & WRITE) {
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    APOLLO_ASSERT(fd_ctx->events == 0);
    return true;
}

// 通知调度器有新任务
void IOManager::tickle() {
    // 如果没有空闲线程，则不执行tickle了
    if(!hasIdleThreads()) {
        return;
    }

    int rt = write(m_tickleFds[1], "T", 1);
    APOLLO_ASSERT(rt == 1);
}

// 判断是否可以停止
bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}

bool IOManager::stopping(uint64_t& timeout) {
    timeout = getNextTimer();
    return timeout == ~0ull
        && Scheduler::stopping()
        && m_pendingEventCount == 0;
}

// 协程无任务可调度时切换回ide协程
void IOManager::idle() {
    APOLLO_LOG_DEBUG(g_logger) << "idle";

    // 一次epoll_wait最多检测256个就绪事件，如果就绪事件超过了这个数，那么会在下轮epoll_wati继续处理
    const uint64_t MAX_EVNETS = 256;
    epoll_event* events = new epoll_event[MAX_EVNETS]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
        delete[] ptr;
    });

    while(true) {
        uint64_t next_timeout = 0;
        if(APOLLO_UNLIKELY(stopping(next_timeout))) {
            APOLLO_LOG_INFO(g_logger) << "name = " << getName()
                                    << " idle stopping exit";
            break;
        }

        // 阻塞在epoll_wait上，等待事件发生
        int rt = 0;
        do {
            static const int MAX_TIMEOUT = 5000;
            if(next_timeout != ~0ull) {
                next_timeout = (int)next_timeout > MAX_TIMEOUT
                            ? MAX_TIMEOUT : next_timeout;
            } else {
                next_timeout = MAX_TIMEOUT;
            }
            // epoll_wait返回的是待处理事件的长度
            rt = epoll_wait(m_epfd, events, MAX_EVNETS, (int)next_timeout);
            // APOLLO_LOG_INFO(g_logger) << "<><>Epoll_wait rt = " << rt;
            if(rt < 0 && errno == EINTR) {
            } else {
                break;
            }
        } while(true);

        // 拿出所有已经超时的定时器，其cb全部执行
        std::vector<std::function<void()>> cbs;
        listExpiredCbs(cbs);
        if(!cbs.empty()) {
            // APOLLO_LOG_DEBUG(g_logger) << "size of cbs = " << cbs.size();
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        for(int i = 0; i < rt; ++i) {
            epoll_event& event = events[i];
            if(event.data.fd == m_tickleFds[0]) {
                // ticklefd[0]用于通知协程调度，这时只需要把管道里的内容读完即可，本轮idle结束Scheduler::run会重新执行协程调度
                uint8_t dummy[256];
                while(read(m_tickleFds[0], dummy, sizeof(dummy)) > 0)
                    ;
                continue;
            }

            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            if(event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }

            int real_events = NONE;

            if(event.events & EPOLLIN) {
                real_events |= READ;
            }

            if(event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            if((fd_ctx->events & real_events) == NONE) {
                continue;
            }

            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;

            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2) {
                APOLLO_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                    << (EpollCtlOp)op << ", " << fd_ctx->fd << ", " << (EPOLL_EVENTS)event.events << "):"
                    << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            if(real_events & READ) {
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            if(real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut();
    }
}

// 重置socket句柄上下文的容器大小
void IOManager::resizeContext(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); i++) {
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

// 获取事件上下文
IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event) {
    switch(event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            APOLLO_ASSERT2(false, "Get Context Failed");
    }
    throw std::invalid_argument("Get Invalid Context Event");
}

// 重置事件上下文
void IOManager::FdContext::resetContext(EventContext& ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

// 触发事件
void IOManager::FdContext::triggerEvent(Event event) {
    APOLLO_LOG_INFO(g_logger) << "Call Trigger Event : " << event << "========";
    APOLLO_ASSERT(events & event);
    events = (Event)(events & ~event);
    EventContext& ctx = getContext(event);
    if(ctx.cb) {
        ctx.scheduler->schedule(&ctx.cb);
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    ctx.scheduler = nullptr;
    return;
}

// 当有新的定时器插入到了列表首部，需要通知调度器
void IOManager::onTimerInsertAtFront() {
    tickle();
}

}