#include "log.h"
#include <map>
#include <iostream>
#include <functional>
#include <time.h>
#include <string.h>

namespace apollo
{
// -------------------------------------------------------
// LogEventWrap 对日志事件的封装
LogEventWrap::LogEventWrap(LogEvent::ptr e) 
        : m_event(e) {    
}

// 基于智能指针释放内存，调用析构函数，从而出发日志写入
// 否则，如果不适用Wrap的话，日志将会一直在main程序执行完成后才进行释放
LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(), m_event);
}

std::stringstream& LogEventWrap::getSS() {
    return m_event->getSS();
}

// -------------------------------------------------------
// Loggerformatter overide
// 日志内容
class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class NameFormatItem : public LogFormatter::FormatItem {
public:
    NameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLogger()->getName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadName();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
        :m_format(format) {
        if(m_format.empty()) {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_format;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
public:
    FilenameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFile();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << std::endl;
    }
};


class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string& str)
        :m_string(str) {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }
private:
    std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << "\t";
    }
private:
    std::string m_string;
};


// --------------------------------------------------------
// LogEvent implementation
LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level
            ,const char* file, int32_t line, uint32_t elapse
            ,uint32_t thread_id, uint32_t fiber_id, uint64_t time
            ,const std::string& thread_name)
        : m_file(file) 
        , m_line(line)
        , m_elapse(elapse)
        , m_threadId(thread_id)
        , m_fiberId(fiber_id)
        , m_time(time)
        , m_threadName(thread_name)
        , m_logger(logger)
        , m_level(level) {
}

/**
 *  格式化写入日志内容
 */
void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

/**
 *  格式化写入日志内容
 */
void LogEvent::format(const char* fmt, va_list al) {
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if(len != -1) {
        m_ss << std::string(buf, len);  // 写入内容
        free(buf);
    }
}

// --------------------------------------------------------
// Logger implementation
Logger::Logger(const std::string& name)
    :m_name(name)
    ,m_level(LogLevel::DEBUG) {
        // member initialization list动作，效率高于赋值动作
    // (reference by <<effective c++>> item 4)
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::addAppender(LogAppender::ptr appender)
{
    /*
        ****** Bug Here!  *******
        要记得为添加的appender注入formatter对象
    */
    if(!appender->getFormatter()) {
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender)
{
    for (auto it = m_appenders.begin(); // auto (after c++ 11)
        it != m_appenders.end(); ++it)
    {
        if (*it == appender)
        {
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::setFormatter(LogFormatter::ptr val) {
    m_formatter = val;

    for(auto& i : m_appenders) {
        if(!i->m_hasFormatter) {
            i->m_formatter = m_formatter;
        }
    }
}

void Logger::setFormatter(const std::string& val) {
    std::cout << "---" << val << std::endl;
    apollo::LogFormatter::ptr new_val(new apollo::LogFormatter(val));
    if(new_val->isError()) {
        std::cout << "Logger setFormatter name=" << m_name
                << " value=" << val << " invalid formatter"
                << std::endl;
        return;
    }
    //m_formatter = new_val;
    setFormatter(new_val);
}

LogFormatter::ptr Logger::getFormatter() {
    return m_formatter;
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event)
{
    if (level >= m_level)
    {
        auto self = shared_from_this();
        // for (auto &i : m_appenders)
        // {
        //     std::cout << "------- logger has appender ---" << std::endl;
        //     i->log(self, level, event);
        // }

        /*  
            当没有为当前logger添加appender的时候，自动调用"root"日志器打印日志
        */
        if(!m_appenders.empty()) {
            for(auto& i : m_appenders) {
                i->log(self, level, event);
            }
        } 
        else if(m_root) {
            m_root->log(level, event);
        }
    }
}

void Logger::debug(LogEvent::ptr event)
{
    log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event)
{
    log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event)
{
    log(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event)
{
    log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event)
{
    log(LogLevel::FATAL, event);
}

// --------------------------------------------------------
// LogLevel implementation
const char *LogLevel::ToString(LogLevel::Level level)
{
    switch (level)
    {
#define XX(name)         \
case LogLevel::name: \
    return #name;   \
    break;

        XX(DEBUG);
        XX(INFO);
        XX(WARN)
        XX(ERROR);
        XX(FATAL);

#undef XX
    default:
        return "UNKNOWN";
    }

    return "UNKNOWN";
}

// --------------------------------------------------------
// Logappender implementation
LogFormatter::ptr LogAppender::getFormatter() {
    return m_formatter;
}

void LogAppender::setFormatter(LogFormatter::ptr val) {
    m_formatter = val;
    if(m_formatter) {
        m_hasFormatter = true;
    } 
    else {
        m_hasFormatter = false;
    }
}

FileLogAppender::FileLogAppender(const std::string& filename)
    : m_filename(filename){
    reopen();
}

void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event)
{
    if(level >= m_level) {
        uint64_t now = event->getTime();
        if(now >= (m_lastTime + 3)) {
            reopen();
            m_lastTime = now;
        }
        //if(!(m_filestream << m_formatter->format(logger, level, event))) {
        if(!m_formatter->format(m_filestream, logger, level, event)) {
            std::cout << "error" << std::endl;
        }
    }
}

bool FileLogAppender::reopen()
{
    if (m_filestream)
    {
        m_filestream.close();
    }

    m_filestream.open(m_filename);

    return !!m_filestream; //  将非零值转成1，0值不变
}

void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event)
{
    if (level >= LogAppender::getLevel())
    {
        // 段错误BUG的触发地
        // 对应到函数Logger::addAppender， 如果addAppender中添加的appender中没有添加formatter，则这里会出现空指针引用
        m_formatter->format(std::cout, logger, level, event);
    }
}

// --------------------------------------------------------
// Logformatter implementation
LogFormatter::LogFormatter(const std::string &pattern)
    : m_pattern(pattern){
    init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for(auto& i : m_items) {
        i->format(ss, logger, level, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    for(auto& i : m_items) {
        i->format(ofs, logger, level, event);
    }
    return ofs;
}

// 日志输出格式初始化
void LogFormatter::init()
{
    // str, format, type
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;
    for (size_t i = 0; i < m_pattern.size(); ++i)
    {
        if (m_pattern[i] != '%')
        {
            nstr.append(1, m_pattern[i]);
            continue;
        }

        if ((i + 1) < m_pattern.size())
        {
            if (m_pattern[i + 1] == '%')
            {
                nstr.append(1, '%');
                continue;
            }
        }

        size_t n = i + 1;
        int fmt_status = 0;
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;
        while (n < m_pattern.size())
        {
            if (!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}'))
            {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if (fmt_status == 0)
            {
                if (m_pattern[n] == '{')
                {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    // std::cout << "*" << str << std::endl;
                    fmt_status = 1; //解析格式
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            }
            else if (fmt_status == 1)
            {
                if (m_pattern[n] == '}')
                {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    // std::cout << "#" << fmt << std::endl;
                    fmt_status = 0;
                    ++n;
                    break;
                }
            }
            ++n;
            if (n == m_pattern.size())
            {
                if (str.empty())
                {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if (fmt_status == 0)
        {
            if (!nstr.empty())
            {
                vec.push_back(std::make_tuple(nstr, std::string(), 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        }
        else if (fmt_status == 1)
        {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
        }
    }

    if (!nstr.empty())
    {
        vec.push_back(std::make_tuple(nstr, "", 0));
    }
    static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> s_format_items = {
#define XX(str, C)                                                               \
{                                                                            \
#str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); } \
}

        XX(m, MessageFormatItem),  // m:消息
        XX(p, LevelFormatItem),    // p:日志级别
        XX(r, ElapseFormatItem),   // r:累计毫秒数
        XX(c, NameFormatItem),     // c:日志名称
        XX(t, ThreadIdFormatItem), // t:线程id
        XX(n, NewLineFormatItem),  // n:换行
        XX(d, DateTimeFormatItem), // d:时间
        XX(f, FilenameFormatItem), // f:文件名
        XX(l, LineFormatItem),     // l:行号
        XX(T, TabFormatItem),      // T:Tab
        XX(F, FiberIdFormatItem),  // F:协程id
        XX(N, ThreadNameFormatItem),      // N:线程名称
#undef XX
    };

    for (auto &i : vec)
    {
        if (std::get<2>(i) == 0)
        {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        }
        else
        {
            auto it = s_format_items.find(std::get<0>(i));
            if (it == s_format_items.end())
            {
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            }
            else
            {
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }

        // std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")" << std::endl;
    }
    // std::cout << m_items.size() << std::endl;
}

// ----------------------------------------------------
// LoggerManager implementation
LoggerManager::LoggerManager() {
    m_root.reset(new Logger);
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));

    m_loggers[m_root->m_name] = m_root;

    // std::cout << "----- was called ctor here ---" << std::endl;

    // std::cout << "m_root: " << m_root << std::endl;

    init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) {
        return it->second;
    }

    // std::cout << "print logger name: " << name << std::endl;

    Logger::ptr logger(new Logger(name));
    /* 
        每个logger里都会有一个名为“root”的主logger，当没有为当前logger设置appender时，会自动调用“root” logger进行日志处理
        经由日志管理器生成的logger中m_root的root都是一样的，只有一个
    */
    logger->m_root = m_root;   
    m_loggers[name] = logger;

    // std::cout << "logger: " << logger << std::endl;
    
    // std::cout << "logger->m_root: " << logger->m_root << std::endl;

    // std::cout << "logger->getLevel: " << LogLevel::ToString(logger->getLevel()) << std::endl;

    return logger;
}

void LoggerManager::init() {
}

}