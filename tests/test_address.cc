#include "src/apollo.h"

static apollo::Logger::ptr g_logger = APOLLO_LOG_ROOT();

void test() {
    std::vector<apollo::Address::ptr> addrs;

    APOLLO_LOG_INFO(g_logger) << "begin";
    // bool v = apollo::Address::Lookup(addrs, "localhost:3080");
    bool v = apollo::Address::Lookup(addrs, "www.baidu.com", AF_INET);
    // bool v = apollo::Address::Lookup(addrs, "www.sylar.top", AF_INET);
    APOLLO_LOG_INFO(g_logger) << "end";
    if(!v) {
        APOLLO_LOG_ERROR(g_logger) << "lookup fail";
        return;
    }

    for(size_t i = 0; i < addrs.size(); ++i) {
        APOLLO_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }

    auto addr = apollo::Address::LookupAny("localhost:4080");
    if(addr) {
        APOLLO_LOG_INFO(g_logger) << *addr;
    } else {
        APOLLO_LOG_ERROR(g_logger) << "error";
    }
}

void test_iface() {
    std::multimap<std::string, std::pair<apollo::Address::ptr, uint32_t> > results;

    bool v = apollo::Address::GetInterfaceAddresses(results);
    if(!v) {
        APOLLO_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    for(auto& i: results) {
        APOLLO_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - "
            << i.second.second;
    }
}

void test_ipv4() {
    //auto addr = apollo::IPAddress::Create("www.sylar.top");
    auto addr = apollo::IPAddress::Create("127.0.0.8");
    if(addr) {
        APOLLO_LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char** argv) {
    test_ipv4();
    // test_iface();
    // test();
    return 0;
}
