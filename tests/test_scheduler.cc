#include "../src/apollo.h"

static apollo::Logger::ptr g_logger = APOLLO_LOG_ROOT();

void test_fiber() {
    static int s_count = 5;
    APOLLO_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if(--s_count >= 0) {
        apollo::Scheduler::GetThis()->schedule(&test_fiber, apollo::GetThreadId());
    }
}

int main(int argc, char** argv) {
    APOLLO_LOG_INFO(g_logger) << "-----  main  -----";
    apollo::Scheduler sc(3, true, "test");
    sc.start();
    sleep(2);
    APOLLO_LOG_INFO(g_logger) << "-----  schedule  -----";
    sc.schedule(&test_fiber);
    sc.stop();
    APOLLO_LOG_INFO(g_logger) << "-----  over  -----";
    return 0;
}
