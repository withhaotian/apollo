#include <map>
#include <iostream>
#include <functional>
#include <time.h>
#include <string.h>
#include <yaml-cpp/yaml.h>
#include <ctype.h>

#include "log.h"
#include "config.h"

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

void Logger::clearAppenders() {
    m_appenders.clear();
}

void Logger::addAppender(LogAppender::ptr appender)
{
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
        std::cout << "LOGGER SETFORMATTER NAME: " << m_name
                << " VALUE: " << val << " IS THE INVALID FORMATTER"
                << std::endl;
        return;
    }
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

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v) \
    if(str == #v) { \
        return LogLevel::level; \
    }
    // 处理为小写的情况
    XX(DEBUG, debug);
    XX(INFO, info);
    XX(ERROR, error);
    XX(WARN, warn);
    XX(FATAL, fatal);
    
    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(ERROR, ERROR);
    XX(WARN, WARN);
    XX(FATAL, FATAL);
    return LogLevel::UNKNOWN;
#undef XX
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

        XX(m, MessageFormatItem),       // m:消息
        XX(p, LevelFormatItem),         // p:日志级别
        XX(r, ElapseFormatItem),        // r:累计毫秒数
        XX(c, NameFormatItem),          // c:日志名称
        XX(t, ThreadIdFormatItem),      // t:线程id
        XX(n, NewLineFormatItem),       // n:换行
        XX(d, DateTimeFormatItem),      // d:时间
        XX(f, FilenameFormatItem),      // f:文件名
        XX(l, LineFormatItem),          // l:行号
        XX(T, TabFormatItem),           // T:Tab
        XX(F, FiberIdFormatItem),       // F:协程id
        XX(N, ThreadNameFormatItem),    // N:线程名称
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
    }
}

// ---------------------------------------------------
// LoggerManager implementation
LoggerManager::LoggerManager() {
    m_root.reset(new Logger);
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));

    m_loggers[m_root->m_name] = m_root;

    init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) {
        return it->second;
    }

    Logger::ptr logger(new Logger(name));
    /* 
        每个logger里都会有一个名为“root”的主logger，当没有为当前logger设置appender时，会自动调用“root” logger进行日志处理
        经由日志管理器生成的logger中m_root的root都是一样的，只有一个
    */
    logger->m_root = m_root;   
    m_loggers[name] = logger;

    return logger;
}


// ----------------------------------------------------------
// 自定义log
struct LogAppenderDefine {
    int type = 0;   // 1-FileAppender, 2-StdoutAppender
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine& oth) const {
        return type == oth.type
            && level == oth.level
            && formatter == oth.formatter
            && file == oth.file;
    }
};

// Log类的配置解析
struct LogDefine {
    std::string name;   // 日志名称
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine& oth) const {
        return name == oth.name
            && level == oth.level
            && formatter == oth.formatter
            && appenders == oth.appenders;
    }

    bool operator<(const LogDefine& oth) const {
        return name < oth.name;
    }
};

/* 模板类偏特化，从log到string的转换，解析yaml文件 */
template<>
class LexicalCast<std::string, LogDefine> {
public:
   LogDefine operator()(const std::string& v) {
        YAML::Node n = YAML::Load(v);
        LogDefine ld;
        if(!n["name"].IsDefined()) {
            std::cout << "LOG CONFIG ERROR, LOG NAME IS NULL " << n
                      << std::endl;
            throw std::logic_error("LOG CONFIG NAME IS NULL!");
        }
        ld.name = n["name"].as<std::string>();
        ld.level = LogLevel::FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
        if(n["formatter"].IsDefined()) {
            ld.formatter = n["formatter"].as<std::string>();
        }

        if(n["appenders"].IsDefined()) {
            for(size_t x = 0; x < n["appenders"].size(); ++x) {
                auto a = n["appenders"][x];
                if(!a["type"].IsDefined()) {
                    std::cout << "LOG CONDIG ERROR, LOG APPENDER TYPE IS NULL " << a
                              << std::endl;
                    continue;
                }
                std::string type = a["type"].as<std::string>();
                LogAppenderDefine lad;
                if(type == "FileLogAppender") {
                    lad.type = 1;
                    if(!a["file"].IsDefined()) {
                        std::cout << "LOG CONFIG ERROR, FILE PATH IS NULL " << a
                              << std::endl;
                        continue;
                    }
                    lad.file = a["file"].as<std::string>();
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else if(type == "StdoutLogAppender") {
                    lad.type = 2;
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else {
                    std::cout << "LOG CONFIG ERROR, LOG APPENDER IS INVALID " << a
                              << std::endl;
                    continue;
                }
                if(a["level"].IsDefined()) {
                    lad.level = LogLevel::FromString(a["level"].as<std::string>()); 
                } else {
                    lad.level = LogLevel::UNKNOWN;
                }


                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

template<>
class LexicalCast<LogDefine, std::string> {
public:
    std::string operator()(const LogDefine& i) {
        YAML::Node n;
        n["name"] = i.name;
        if(i.level != LogLevel::UNKNOWN) {
            n["level"] = LogLevel::ToString(i.level);
        }
        if(!i.formatter.empty()) {
            n["formatter"] = i.formatter;
        }

        for(auto& a : i.appenders) {
            YAML::Node na;
            if(a.type == 1) {
                na["type"] = "FileLogAppender";
                na["file"] = a.file;
            } else if(a.type == 2) {
                na["type"] = "StdoutLogAppender";
            }
            if(a.level != LogLevel::UNKNOWN) {
                na["level"] = LogLevel::ToString(a.level);
            }

            if(!a.formatter.empty()) {
                na["formatter"] = a.formatter;
            }

            n["appenders"].push_back(na);
        }
        std::stringstream ss;
        ss << n;
        return ss.str();
    }
};

// 定义全局的日志配置
apollo::ConfigVar<std::set<LogDefine>>::ptr g_log_defines = 
    apollo::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

// 全局日志监听器，初始化于manin函数发生之前
struct LogIniter {
    LogIniter() {
        g_log_defines->addLisiner(
            [](const std::set<LogDefine>& old_vals, const std::set<LogDefine>& new_vals) {
        APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "ON LOG CONFIG CHANGES";
        // 监听logger配置的变更
        // 1. 新增logger
        // 2. 修改logger
        // 3. 删除logger
        for(auto& i : new_vals) {
            auto it = old_vals.find(i);

            apollo::Logger::ptr logger;
            if(it == old_vals.end()) {
                // 添加新logger
                logger = APOLLO_LOG_NAME(i.name);   // 获取到logger
            } else {
                if(!(i == *it)) {
                    // 当前lodder i被修改
                    logger = APOLLO_LOG_NAME(i.name);
                } else {continue;}
            }

            // 写入值
            logger->setLevel(i.level);
            if(!i.formatter.empty()) {
                logger->setFormatter(i.formatter);
            }

            logger->clearAppenders();       // 先清空logger中的所有appender
            for(auto& x : i.appenders) {
                apollo::LogAppender::ptr ap;
                if(x.type == 1) { // FileAppender
                    ap.reset(new FileLogAppender(x.file));
                } else if(x.type == 2) {
                    ap.reset(new StdoutLogAppender());
                } else {continue;}

                ap->setLevel(x.level);
                if(!x.formatter.empty()) {
                    apollo::LogFormatter::ptr fmt(new apollo::LogFormatter(x.formatter));
                    if(!fmt->isError()) {
                        ap->setFormatter(fmt);
                    } else {
                        std::cout << "LOG.NAME = " << i.name << " APPENDER TYPE = " << x.type
                            << " FORMATTER = " << x.formatter << " IS INVALID" << std::endl;
                    }
                }
                // 添加appender于logger中
                logger->addAppender(ap);
            }
        }

        for(auto& i : old_vals) {
            auto it = new_vals.find(i);
            if(it == new_vals.end()) {
                // clear logger的所有参数
                auto logger = APOLLO_LOG_NAME(i.name);
                logger->setLevel(apollo::LogLevel::UNKNOWN);
                logger->clearAppenders();
            }
        }

        });
    } 
};

// 静态变量，log初始化（并且发生在main函数之前）
static LogIniter __log_init;

// -------------------------------------------------
// 输出yaml配置
std::string FileLogAppender::toYamlString() {
    YAML::Node node;

    node["type"] = "FileLogAppender";
    if(m_level != LogLevel::UNKNOWN)
        node["level"] = LogLevel::ToString(m_level);
    
    if(m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }

    node["file"] = m_filename;

    std::stringstream ss;
    ss << node;

    return ss.str();
}

std::string StdoutLogAppender::toYamlString() {
    YAML::Node node;

    node["type"] = "StdoutLogAppender";
    if(m_level != LogLevel::UNKNOWN)
        node["level"] = LogLevel::ToString(m_level); 

    if(m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }

    std::stringstream ss;
    ss << node;

    return ss.str();

}

std::string Logger::toYamlString() {
    YAML::Node node;

    node["name"] = m_name;
    if(m_level != LogLevel::UNKNOWN)
        node["level"] = LogLevel::ToString(m_level); 

    if(m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }

    for(auto& i : m_appenders) {
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }

    std::stringstream ss;
    ss << node;

    return ss.str();
}

std::string LoggerManager::toYamlString() {
    YAML::Node node;

    for(auto& i : m_loggers) {
        node.push_back(YAML::Load(i.second->toYamlString()));
    }

    std::stringstream ss;
    ss << node;

    return ss.str();

}

}  // namespace apollo

