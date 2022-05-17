#ifndef __APOLLO_IOMANAGER_H__
#define __APOLLO_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace apollo
{

class IOManager : public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;
    
    enum Event {
        NONE = 0x0,
        READ = 0x1,        // = EPOLLIN
        WRITE = 0x4,       // = EPOLLOUT
    };

private:
    // 事件上下文
    struct FdContext {
        typedef Mutex MutexType;
        struct EventContext {
            // 事件执行的scheduler
            Scheduler* scheduler = nullptr;
            // 事件的协程
            Fiber::ptr fiber;
            // 事件的执行函数
            std::function<void()> cb;
        };
        
        // 获取事件上下文
        EventContext& getContext(IOManager::Event event);

        // 重置事件上下文
        void resetContext(EventContext& ctx);

        // 触发事件
        void triggerEvent(IOManager::Event event);

        // 读事件
        EventContext read;
        // 写事件
        EventContext write;
        // 事件关联的句柄
        int fd;
        // 已注册的事件
        Event events = NONE;
        MutexType mutex;
    };

public:
    // 构造函数
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");

    ~IOManager();

    // 添加事件， 0 - success， -1 - error
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    
    // 删除事件
    bool delEvent(int fd, Event event);

    // 取消事件
    bool cancelEvent(int fd, Event event);

    // 取消所有事件
    bool cancelAll(int fd);

    // 获取当前IOManager
    static IOManager* GetThis();

protected:
    // 通知调度器有新任务
    void tickle() override;

    // 判断是否可以停止
    bool stopping() override;
    /**
     * @brief 判断是否可以停止
     * @param[out] timeout 最近要发生的定时器事件间隔
     * @return 返回是否可以停止
     */
    bool stopping(uint64_t& timeout);

    // 协程无任务可调度时切换回ide协程
    void idle() override;

    // 重置socket句柄上下文的容器大小
    void resizeContext(size_t size);

    // 当有新的定时器插入到了列表首部，需要通知调度器
    void onTimerInsertAtFront() override;

private:
    // IOMaager 读写锁
    RWMutex m_mutex;
    
    // epoll 文件句柄
    int m_epfd = 0;
    
    // pipe 文件句柄
    int m_tickleFds[2];

    // 当前等待执行的事件数量
    std::atomic<size_t> m_pendingEventCount = {0};
    
    // socket事件上下文的容器
    std::vector<FdContext*> m_fdContexts;
};

} // namespace apollo


#endif