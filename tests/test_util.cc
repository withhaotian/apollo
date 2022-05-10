#include "../src/apollo.h"
#include <assert.h>

apollo::Logger::ptr g_logger = APOLLO_LOG_ROOT();

void test_assert() {
    APOLLO_LOG_INFO(g_logger) << apollo::BacktraceToString(10);
    APOLLO_ASSERT2(0 == 1, "TEST APOLLO_ASSERT2: ");
}

int main(int argc, char** argv) {
    test_assert();
    return 0;
}
