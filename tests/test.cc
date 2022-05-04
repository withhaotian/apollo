#include <iostream>
#include "../src/log.h"
#include "../src/util.h"

int main(int argc, char** argv) {
    apollo::Logger::ptr logger(new apollo::Logger);
    logger->addAppender(apollo::LogAppender::ptr(new apollo::StdoutLogAppender));

    // log写入文件
    apollo::FileLogAppender::ptr file_appender(new apollo::FileLogAppender("log.txt"));
    apollo::LogFormatter::ptr fmt(new apollo::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setFormatter(fmt);
    file_appender->setLevel(apollo::LogLevel::ERROR);

    logger->addAppender(file_appender);

    std::cout << "hello apollo log" << std::endl;

    APOLLO_LOG_INFO(logger) << "test macro";
    APOLLO_LOG_ERROR(logger) << "test macro error";

    APOLLO_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

    auto loggermgr = apollo::LoggerMgr::GetInstance()->getLogger("apolxo");
    // std::cout << loggermgr << " ***** " << std::endl;
    APOLLO_LOG_FATAL(loggermgr) << "test logger manager";

    auto loggermgr2 = apollo::LoggerMgr::GetInstance()->getLogger("root");
    // std::cout << loggermgr << " ***** " << std::endl;
    APOLLO_LOG_INFO(loggermgr2) << "test root logger manager";
    
    return 0;
}