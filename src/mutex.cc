#include "mutex.h"
#include <stdexcept>

namespace apollo
{
// 初始化信号量
Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&m_semaphore, 0, count)) {
        throw std::logic_error("SEM_INIT ERROR");
    }
}

// 销毁信号量
Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

// 获取信号量, 信号量--
void Semaphore::wait() {
    if(sem_wait(&m_semaphore)) {
        throw std::logic_error("SEM_WAIT ERROR");
    }
}

// 释放信号量, 信号量++
void Semaphore::notify() {
    if(sem_post(&m_semaphore)) {
        throw std::logic_error("SEM_POST ERROR");
    }
}

} // namespace apollo
