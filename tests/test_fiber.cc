#include "../src/apollo.h"
#include <vector>

static apollo::Logger::ptr g_logger = APOLLO_LOG_ROOT();

void run_fiber()
{
    APOLLO_LOG_INFO(g_logger) << "{test_fiber} run_in_fiber begins";
    apollo::Fiber::YieldToHold();           // 中断
    APOLLO_LOG_INFO(g_logger) << "{test_fiber} run_in_fiber ends";
    apollo::Fiber::YieldToHold();
}

void test_fiber()
{
    APOLLO_LOG_INFO(g_logger) << "{test_fiber} Thread BEGINS";
    {
        apollo::Fiber::GetThis();
        APOLLO_LOG_INFO(g_logger) << "{test_fiber} MAIN Fiber BEGINS";
        // APOLLO_LOG_INFO(g_logger) << "^^^^^ Fiber ID: " << apollo::GetFiberId();
        apollo::Fiber::ptr fiber(new apollo::Fiber(run_fiber));
        fiber->swapIn();
        APOLLO_LOG_INFO(g_logger) << "{test_fiber} main after swapIn";
        fiber->swapIn();
        APOLLO_LOG_INFO(g_logger) << "{test_fiber} main after end";
        fiber->swapIn();
    }
    APOLLO_LOG_INFO(g_logger) << "{test_fiber} Thread ENDS";
}

int main(int agrc, char** agrv)
{
    apollo::Thread::SetName("main");
    APOLLO_LOG_INFO(g_logger) << apollo::Thread::GetName();

    std::vector<apollo::Thread::ptr> thrs;
    for(int i = 0; i < 3; ++i) {
        thrs.push_back(apollo::Thread::ptr(
                    new apollo::Thread(&test_fiber, "name_" + std::to_string(i))));
    }
    for(auto i : thrs) {
        i->join();
    }

    return 0;
}