#include "../src/apollo.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>

static apollo::Logger::ptr g_logger = APOLLO_LOG_ROOT();

int sock = 0;

void test_fiber() {
    APOLLO_LOG_INFO(g_logger) << "test_iomanager sock=" << sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "10.14.61.234", &addr.sin_addr.s_addr);

    if(!connect(sock, (const sockaddr*)&addr, sizeof(addr))) {
        // Nothing to do
    } else if(errno == EINPROGRESS) {
        APOLLO_LOG_INFO(g_logger) << "add event errno=" << errno << " " << strerror(errno);
        apollo::IOManager::GetThis()->addEvent(sock, apollo::IOManager::READ, [](){
            APOLLO_LOG_INFO(g_logger) << "read callback";
        });
        apollo::IOManager::GetThis()->addEvent(sock, apollo::IOManager::WRITE, [](){
            APOLLO_LOG_INFO(g_logger) << "write callback";
            // close(sock);
            apollo::IOManager::GetThis()->cancelEvent(sock, apollo::IOManager::READ);
            close(sock);
        });
    } else {
        APOLLO_LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }
}

void test1() {
    std::cout << "EPOLLIN=" << EPOLLIN
            << " EPOLLOUT=" << EPOLLOUT << std::endl;
    apollo::IOManager iom(2, false, "test");
    iom.schedule(&test_fiber);
}

void test_timer() {
    apollo::IOManager iom(2, false);
    apollo::Timer::ptr timer = iom.addTimer(1000, [&timer](){
        static int i = 0;
        APOLLO_LOG_INFO(g_logger) << "hello timer i = " << i;
        if(++i == 3) {
            // s_timer->reset(2000, true);
            timer->cancel();
        }
    }, true);
}

int main(int agrc, char** argv) {
    // test1();
    test_timer();
    return 0;
}