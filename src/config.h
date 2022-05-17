#ifndef __APOLLO_CONFIG_H__
#define __APOLLO_CONFIG_H__

#include <memory>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <ctype.h>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <functional>

#include "log.h"
#include "util.h"
#include "thread.h"
#include "mutex.h"

namespace apollo {
/*
    config基类
*/
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    ConfigVarBase(const std::string& name, const std::string& description = "")
        : m_name(name)
        , m_description(description) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }

    virtual ~ConfigVarBase() {};

    const std::string& getName() const {return m_name;};
    const std::string& getDescription() const {return m_description;};

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& val) = 0;

    virtual std::string getTypeName() const = 0;
protected:
    std::string m_name;
    std::string m_description;
};

/*
 * 类型转换模板类（从F类型转换到T类型） 
*/
template<class F, class T>
class LexicalCast {
public:
    /**
     * 类型转换
     * 参数：源类型值
     * 返回值：v转换后的目标类型
     * 当类型不可转换时抛出异常
     */
    T operator()(const F& v) {
        return boost::lexical_cast<T>(v);
    }
};

/**
 *  类型转换模板类偏特化(YAML String 转换成 std::vector<T>)
 */
template<class T>
class LexicalCast<std::string, std::vector<T> > {
public:
    std::vector<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/**
 * 类型转换模板类偏特化(std::vector<T> 转换成 YAML String)
 */
template<class T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * 类型转换模板类偏特化(YAML String 转换成 std::list<T>)
 */
template<class T>
class LexicalCast<std::string, std::list<T> > {
public:
    std::list<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::list<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/**
 * 类型转换模板类偏特化(std::list<T> 转换成 YAML String)
 */
template<class T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * 类型转换模板类偏特化(YAML String 转换成 std::set<T>)
 */
template<class T>
class LexicalCast<std::string, std::set<T> > {
public:
    std::set<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::set<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/**
 * 类型转换模板类偏特化(std::set<T> 转换成 YAML String)
 */
template<class T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator()(const std::set<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * 类型转换模板类偏特化(YAML String 转换成 std::unordered_set<T>)
 */
template<class T>
class LexicalCast<std::string, std::unordered_set<T> > {
public:
    std::unordered_set<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_set<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

/**
 * 类型转换模板类偏特化(std::unordered_set<T> 转换成 YAML String)
 */
template<class T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T>& v) {
        YAML::Node node(YAML::NodeType::Sequence);
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * 类型转换模板类偏特化(YAML String 转换成 std::map<std::string, T>)
 */
template<class T>
class LexicalCast<std::string, std::map<std::string, T> > {
public:
    std::map<std::string, T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::map<std::string, T> vec;
        std::stringstream ss;
        for(auto it = node.begin();
                it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(),
                        LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

/**
 * 类型转换模板类偏特化(std::map<std::string, T> 转换成 YAML String)
 */
template<class T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    std::string operator()(const std::map<std::string, T>& v) {
        YAML::Node node(YAML::NodeType::Map);
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * 类型转换模板类偏特化(YAML String 转换成 std::unordered_map<std::string, T>)
 */
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T> > {
public:
    std::unordered_map<std::string, T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> vec;
        std::stringstream ss;
        for(auto it = node.begin();
                it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(),
                        LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

/**
 * 类型转换模板类偏特化(std::unordered_map<std::string, T> 转换成 YAML String)
 */
template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string, T>& v) {
        YAML::Node node(YAML::NodeType::Map);
        for(auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/*
    配置参数模板子类,保存对应类型的参数值
    T：参数类型
    FromStr：从string转为T的仿函数, FromStr T operator() (const std::sring&)
    ToStr：从T转为string的仿函数, Tostr std::string operator() (const T&)
*/
template<class T, class FromStr = LexicalCast<std::string, T>,
                  class ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    // 配置项变更的回调函数封装
    typedef std::function<void (const T& old_val, const T& new_val)> on_change_cb;

    typedef RWMutex RWMutexType;

    ConfigVar(const std::string& name,
                const T& default_val,
                const std::string& description = "")
            : ConfigVarBase(name, description)
            , m_val(default_val) {
    }

    // 从参数值转为string
    std::string toString() override {
        try {
            // return boost::lexical_cast<std::string>(m_val);
            RWMutexType::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        } catch(const std::exception& e) {
            APOLLO_LOG_ERROR(APOLLO_LOG_ROOT()) << "ConfigVar::toString EXCEPTION "
            << e.what() << "CONVERT " << typeid(T).name() 
            << " TO STRING FAILED";
        }

        return "";
    }

    // 从string获得参数值
    bool fromString(const std::string& val) override {
        try {
            setValue(FromStr()(val));
        } catch(const std::exception& e) {
            APOLLO_LOG_ERROR(APOLLO_LOG_ROOT()) << "ConfigVar::fromString EXCEPTION "
            << e.what() << "CONVERT STRING" << val
            << " TO " << typeid(T).name() << " FAILED";
        }

        return false;
    }

    // GET/SET方法
    const T& getValue() const {
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    void setValue(const T& val) {
        {
            // 查找用读锁,加入大括号给与作用域
            RWMutexType::ReadLock lock(m_mutex);
            if(val == m_val) {
                return;
            }

            for(auto& i : m_cbs) {
                i.second(m_val, val);  // 写入监听事件，old_val, new_val
            }
        }

        RWMutexType::WriteLock lock(m_mutex);
        m_val = val;
    }

    // 获取T的类型名
    std::string getTypeName() const override {return typeid(T).name();}
    
    // 增加/删除监听器 
    uint64_t addListener(const on_change_cb& cb) {
        RWMutexType::WriteLock lock(m_mutex);
        static uint64_t s_key_id = 0;  // 唯一键
        ++s_key_id;

        m_cbs[s_key_id] = cb;
        return s_key_id;
    }

    on_change_cb addListener(const uint64_t key) const {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);

        return it == m_cbs.end() ? nullptr : it->second;
    }

    void delListener(const uint64_t key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }

    void clearListener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }

private:
    // 配置参数
    T m_val;
    // 变更回调函数，key值唯一
    std::map<uint64_t, on_change_cb> m_cbs; 
    // mutex
    mutable RWMutexType m_mutex;
};

/*
    ConfigVar的管理类
    提供方法创建/管理ConfigVar类
*/ 
class Config {
public:
    typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;   // 多态

    typedef RWMutex RWMutexType;

    /**
        获取/创建配置参数
        如果找到配置项，则返回配置项，否则创建
    */
    template<class T>
    static typename ConfigVar<T>::ptr  Lookup(const std::string& name, 
            const T& default_val, const std::string& description) {
         // 采用静态方法获取静态成员而不采用直接定义静态成员s_datas的目的在于
         // 防止s_datas的初始化在Lookup之前（两个静态变量没法得知哪个先被初始化）
         // 从而调用一个未初始化的静态变量
        RWMutexType::WriteLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if(it != GetDatas().end()) {
            // 如果转换失败，返回0
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>> (it->second);

            if(tmp) {
                APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "LOOKUP NAME " << name << " EXISTS";
                return tmp;
            } else {
                APOLLO_LOG_ERROR(APOLLO_LOG_ROOT()) << "LOOKUP NAME " << name << " EXISTS BUT TYPE "
                << typeid(T).name() << " IS INCONSISTENT TO TYPE " << it->second->getTypeName();
                return nullptr;
            }
        }

        // 配置名只包含小写字母、点以及数字
        if(name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._0123456789")
            != std::string::npos) {
                APOLLO_LOG_ERROR(APOLLO_LOG_ROOT()) << "INVALID LOOKUP NAME " << name;
                throw std::invalid_argument(name);
        }

        // 否则，创建该配置项
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_val, description));
        GetDatas()[name] = v;

        return v;
    }

    /**
     * 查找配置参数,返回配置参数的基类
     */
    static ConfigVarBase::ptr LookupBase(const std::string& name);

    /* 初始化YAML::Node配置模块 */
    static void LoadFromYaml(const YAML::Node& root);

    /* 加载配置模块里面的所有配置项 */
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

private:
    /* 获取所有的配置项 */
    static ConfigVarMap& GetDatas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }
    
    /* 获取锁 */
    static RWMutexType& GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }

};

}
#endif  // CONFIG_H
