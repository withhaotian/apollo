#include "hook.h"
#include "config.h"
#include "log.h"
#include "fiber.h"
#include "iomanager.h"
#include "fdmanager.h"
#include "macro.h"

#include <dlfcn.h>

static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");

namespace apollo
{
// 设置socket连接超时时间
static apollo::ConfigVar<int>::ptr g_tcp_connect_timeout =
    apollo::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

static thread_local bool t_hook_enable = false;     // 设置hook局部变量

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init() {
    static bool is_inited = false;
    if(is_inited) {
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
    is_inited = true;
}

static uint64_t s_connect_timeout = -1;
struct _HookIniter {
    _HookIniter() {
        hook_init();
        APOLLO_LOG_DEBUG(g_logger) << "HookIniter Initialized------";

        // 增加回调函数，监听tcp timeout发生更改的事件
        s_connect_timeout = g_tcp_connect_timeout->getValue();

        g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value){
                // 打印log
                APOLLO_LOG_INFO(g_logger) << "TCP Connect Timeout Changed From "
                                        << old_value << " To " << new_value;
                // 更新全局timeout
                s_connect_timeout = new_value;
        });
    }
};

// 静态局部变量，保证在main函数执行之前初始化
static _HookIniter s_hook_initer;

// 当前线程是否hook
bool is_hook_enable() {
    return t_hook_enable;
}

// 将当前线程设置hook状态
void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

} // namespace apollo

// 超时器信息
struct timer_info {
    int cancelled = 0;
};

template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
        uint32_t event, int timeout_so, Args&&... args) {
    if(!apollo::t_hook_enable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    APOLLO_LOG_INFO(g_logger) << "do_io: " << hook_fun_name;

    apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(fd);
    // 如果句柄上下文不存在，执行原函数
    if(!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    // // 如果句柄已经关闭，执行原函数
    if(ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    // // 如果句柄上下文不是上下文或者用户设置了非阻塞，执行原函数
    if(!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    // 取出超时时间
    uint64_t to = ctx->getTimeout(timeout_so);
    // 设置超时条件
    std::shared_ptr<timer_info> tinfo(new timer_info);

// 需要一直读
// 【fix a bug】 之前没有按照sylar的方式，用了while，但是出现错误
// 得思考这里用while(1) 为什么不行？
retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR) {      // 如果读取不成功且是被系统中断的，读了很多次还没读到操作会换错误为EAGAIN
        n = fun(fd, std::forward<Args>(args)...);
    }
    if(n == -1 && errno == EAGAIN) {        // 如果读取不成功且需要再次读的
        apollo::IOManager* iom = apollo::IOManager::GetThis();
        apollo::Timer::ptr timer;
        // 这里为什么要设置为weak_point？
        // —————因为在timer中addConditionTimer的条件参数是weak_ptr
        std::weak_ptr<timer_info> winfo(tinfo);

        if(to != (uint64_t)-1) {    // 超时时间不等于-1
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event]() {      // 添加条件超时定时器
                auto t = winfo.lock();      // 拿出条件并唤醒
                if(!t || t->cancelled) {    // 定时器已经失效或者超时，直接返回
                    return;
                }
                t->cancelled = ETIMEDOUT;           // 否则，该事件就无需再做并且会被取消
                iom->cancelEvent(fd, (apollo::IOManager::Event)(event));
            }, winfo);
        }

        int rt = iom->addEvent(fd, (apollo::IOManager::Event)(event));
        // 添加失败
        if(APOLLO_UNLIKELY(rt)) {
            APOLLO_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                << fd << ", " << event << ")";
            if(timer) {
                timer->cancel();
            }
            return -1;
        } else {
            apollo::Fiber::YieldToHold();
            if(timer) {
                timer->cancel();
            }
            if(tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }

            goto retry;
        }
    }
    return n;
}


// C++中声明以C方式编译
extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds) {
    if(!apollo::t_hook_enable) {
        return sleep_f(seconds);
    }

    // APOLLO_LOG_DEBUG(g_logger) << " --- use the customized sleep func ---";

    // APOLLO_LOG_DEBUG(g_logger) << " --- sleep: " << seconds << " seconds ---";

    apollo::Fiber::ptr fiber = apollo::Fiber::GetThis();
    // APOLLO_LOG_DEBUG(g_logger) << " --- fiberID: " << apollo::GetFiberId();
    apollo::IOManager* iom = apollo::IOManager::GetThis();
    iom->addTimer(seconds * 1000, std::bind((void(apollo::Scheduler::*)
            (apollo::Fiber::ptr, int thread))&apollo::IOManager::schedule
            ,iom, fiber, -1));
    // APOLLO_LOG_DEBUG(g_logger) << " --- m_fibers.size(): " << iom->getNumTasks();
    apollo::Fiber::YieldToHold();
    return 0;
}

int usleep(useconds_t usec) {
    if(!apollo::t_hook_enable) {
        return usleep_f(usec);
    }
    apollo::Fiber::ptr fiber = apollo::Fiber::GetThis();
    apollo::IOManager* iom = apollo::IOManager::GetThis();
    iom->addTimer(usec / 1000, std::bind((void(apollo::Scheduler::*)
            (apollo::Fiber::ptr, int thread))&apollo::IOManager::schedule
            ,iom, fiber, -1));
    apollo::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if(!apollo::t_hook_enable) {
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 /1000;
    apollo::Fiber::ptr fiber = apollo::Fiber::GetThis();
    apollo::IOManager* iom = apollo::IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind((void(apollo::Scheduler::*)
            (apollo::Fiber::ptr, int thread))&apollo::IOManager::schedule
            ,iom, fiber, -1));
    apollo::Fiber::YieldToHold();
    return 0;
}

// socket接口的hook实现，socket用于创建套接字，需要在拿到fd后将其添加到FdManager中
int socket(int domain, int type, int protocol) {
    if(!apollo::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1) {
        return fd;
    }
    apollo::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

// 由于connect有默认的超时，所以只需要实现connect_with_timeout即可
int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
    if(!apollo::t_hook_enable) {
        return connect_f(fd, addr, addrlen);
    }
    apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if(ctx->getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if(n == 0) {
        return 0;
    } else if(n != -1 || errno != EINPROGRESS) {
        return n;
    }

    apollo::IOManager* iom = apollo::IOManager::GetThis();
    apollo::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms != (uint64_t)-1) {
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom]() {
                auto t = winfo.lock();
                if(!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, apollo::IOManager::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, apollo::IOManager::WRITE);
    if(rt == 0) {
        apollo::Fiber::YieldToHold();
        if(timer) {
            timer->cancel();
        }
        if(tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    } else {
        if(timer) {
            timer->cancel();
        }
        APOLLO_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if(!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, apollo::s_connect_timeout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(s, accept_f, "accept", apollo::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0) {
        apollo::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", apollo::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", apollo::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", apollo::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", apollo::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", apollo::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", apollo::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", apollo::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", apollo::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", apollo::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", apollo::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if(!apollo::t_hook_enable) {
        return close_f(fd);
    }

    apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(fd);
    if(ctx) {
        auto iom = apollo::IOManager::GetThis();
        if(iom) {
            iom->cancelAll(fd);
        }
        apollo::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                    return arg;
                }
                if(ctx->getUserNonblock()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!apollo::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            apollo::FdCtx::ptr ctx = apollo::FdMgr::GetInstance()->get(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}       // extern C