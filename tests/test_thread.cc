#include <iostream>

#include "../src/apollo.h"

apollo::Logger::ptr g_logger = APOLLO_LOG_ROOT();

int count = 0;
apollo::Mutex g_mutex;

void fun1() {
    APOLLO_LOG_INFO(g_logger) << "Name: " << apollo::Thread::GetName()
                            << " | This.Name: " << apollo::Thread::GetThis()->getName()
                            << " | Id: " << apollo::GetThreadId()
                            << " | Ihis.Id: " << apollo::Thread::GetThis()->getId();
    for(int i = 0; i < 100000; ++i) {
        apollo::Mutex::Lock lock(g_mutex);  // 声明了这个对象名称叫lock
        ++count;
    }
}

void fun2() {
    while(true) {
        APOLLO_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
        usleep(100000);
    }
}

void fun3() {
    while(true) {
        APOLLO_LOG_INFO(g_logger) << "========================================";
        usleep(100000);
    }
}

int main(int argc, char** argv) {
    APOLLO_LOG_INFO(g_logger) << "Thread test begins --------";
    
    YAML::Node root = YAML::LoadFile("/mnt/h/workSpace/c_cpp_projects/apollo/bin/conf/log.yml");
    apollo::Config::LoadFromYaml(root);

    std::vector<apollo::Thread::ptr> thrs;

    for(int i = 0; i < 2; ++i) {
        apollo::Thread::ptr thr1(new apollo::Thread(&fun2, "name_" + std::to_string(i * 2)));
        apollo::Thread::ptr thr2(new apollo::Thread(&fun3, "name_" + std::to_string(i * 2 + 1)));
        
        thrs.push_back(thr1);
        thrs.push_back(thr2);
    }

    for(size_t i = 0; i < thrs.size(); i++) {
        thrs[i]->join();
    }

    APOLLO_LOG_INFO(g_logger) << "Thread test ends --------";
    APOLLO_LOG_INFO(g_logger) << "count=" << count;

    return 0;
}
