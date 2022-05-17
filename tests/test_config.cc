#include <iostream>
#include <yaml-cpp/yaml.h>

#include "../src/log.h"
#include "../src/util.h"
#include "../src/config.h"

apollo::ConfigVar<int>::ptr g_int_value_config =
    apollo::Config::Lookup("system.port", (int)8080, "system port");

apollo::ConfigVar<float>::ptr g_int_valuex_config =
    apollo::Config::Lookup("system.port", (float)8080, "system port");

apollo::ConfigVar<float>::ptr g_float_value_config =
    apollo::Config::Lookup("system.value", (float)10.2f, "system value");

apollo::ConfigVar<std::vector<int> >::ptr g_int_vec_value_config =
    apollo::Config::Lookup("system.int_vec", std::vector<int>{1,2}, "system int vec");

apollo::ConfigVar<std::list<int> >::ptr g_int_list_value_config =
    apollo::Config::Lookup("system.int_list", std::list<int>{1,2}, "system int list");

apollo::ConfigVar<std::set<int> >::ptr g_int_set_value_config =
    apollo::Config::Lookup("system.int_set", std::set<int>{1,2}, "system int set");

apollo::ConfigVar<std::unordered_set<int> >::ptr g_int_uset_value_config =
    apollo::Config::Lookup("system.int_uset", std::unordered_set<int>{1,2}, "system int uset");

apollo::ConfigVar<std::map<std::string, int> >::ptr g_str_int_map_value_config =
    apollo::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k",2}}, "system str int map");

apollo::ConfigVar<std::unordered_map<std::string, int> >::ptr g_str_int_umap_value_config =
    apollo::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k",2}}, "system str int map");

void print_yaml(const YAML::Node& node, int level) {
    if(node.IsScalar()) {
        APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << std::string(level * 4, ' ')
            << node.Scalar() << " - " << node.Type() << " - " << level;
    } else if(node.IsNull()) {
        APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << std::string(level * 4, ' ')
            << "NULL - " << node.Type() << " - " << level;
    } else if(node.IsMap()) {
        for(auto it = node.begin();
                it != node.end(); ++it) {
            APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << std::string(level * 4, ' ')
                    << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    } else if(node.IsSequence()) {
        for(size_t i = 0; i < node.size(); ++i) {
            APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << std::string(level * 4, ' ')
                << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/mnt/h/workSpace/c_cpp_projects/apollo/bin/conf/log.yml");
    print_yaml(root, 0);
}

void test_config() {    
    APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "before: " << g_int_value_config->getValue();
    APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "before: " << g_float_value_config->toString();

#define XX(g_var, name, prefix) \
    { \
        auto& v = g_var->getValue(); \
        for(auto& i : v) { \
            APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << #prefix " " #name ": " << i; \
        } \
        APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

#define XX_M(g_var, name, prefix) \
    { \
        auto& v = g_var->getValue(); \
        for(auto& i : v) { \
            APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << #prefix " " #name ": {" \
                    << i.first << " - " << i.second << "}"; \
        } \
        APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

    XX(g_int_vec_value_config, int_vec, before);
    XX(g_int_list_value_config, int_list, before);
    XX(g_int_set_value_config, int_set, before);
    XX(g_int_uset_value_config, int_uset, before);
    XX_M(g_str_int_map_value_config, str_int_map, before);
    XX_M(g_str_int_umap_value_config, str_int_umap, before);

    YAML::Node root = YAML::LoadFile("/mnt/h/workSpace/c_cpp_projects/apollo/bin/conf/log.yml");
    apollo::Config::LoadFromYaml(root);

    APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "after: " << g_int_value_config->getValue();
    APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "after: " << g_float_value_config->toString();

    XX(g_int_vec_value_config, int_vec, after);
    XX(g_int_list_value_config, int_list, after);
    XX(g_int_set_value_config, int_set, after);
    XX(g_int_uset_value_config, int_uset, after);
    XX_M(g_str_int_map_value_config, str_int_map, after);
    XX_M(g_str_int_umap_value_config, str_int_umap, after);
}


// 支持非STL容器的自定义类解析
class Person{
public:
    Person(){};
    
    std::string toString() const {
        std::stringstream ss;
        ss << "[Person: name = " << m_name
           << ", age = " << m_age
           << ", sex = " << m_sex
           << " ]";
        return ss.str();
    }

    bool operator==(const Person& oth) const {
        return m_name == oth.m_name
            && m_age == oth.m_age
            && m_sex == oth.m_sex;
    }

    void setName(const std::string& val) {m_name = val;}
    
    std::string getName() const {return m_name;}

    void setAge(const int val) {m_age = val;}
    
    int getAge() const {return m_age;}

    void setSex(const bool val) {m_sex = val;}
    
    bool getSex() const {return m_sex;}

private:
    std::string m_name;
    int m_age = 0;
    bool m_sex = false;
};

// 继续定义类型转化模板类偏特化
namespace apollo {
template<>
class LexicalCast<std::string, Person > {
public:
    Person operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        Person p;
        p.setName(node["name"].as<std::string>());
        p.setAge(node["age"].as<int>());
        p.setSex(node["sex"].as<bool>());
        return p;
    }
};

template<>
class LexicalCast<Person, std::string> {
public:
    std::string operator()(const Person& p) {
        YAML::Node node(YAML::NodeType::Sequence);
        
        node["name"] = p.getName();
        node["age"] = p.getAge();
        node["sex"] = p.getSex(); 

        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
}
apollo::ConfigVar<Person>::ptr g_person = 
    apollo::Config::Lookup("class.person", Person(), "system person");

void test_class() {
    APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "before: " << g_person->toString();
   
    // 添加配置事件修改监听器
    g_person->addListener([](const Person& old_val, const Person& new_val) {
            APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "|old val = " << old_val.toString()
                    << " | new val = " << new_val.toString() << "|";
            });

    YAML::Node root = YAML::LoadFile("/mnt/h/workSpace/c_cpp_projects/apollo/bin/conf/test.yml");
    apollo::Config::LoadFromYaml(root);

    APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "after: " << g_person->toString();
}

void test_log() {
    static apollo::Logger::ptr sys_log = APOLLO_LOG_NAME("system");
    
    std::cout << "============= before ==============" << std::endl;
    std::cout << apollo::LoggerMgr::GetInstance()->toYamlString() << std::endl;

    YAML::Node root = YAML::LoadFile("/mnt/h/workSpace/c_cpp_projects/apollo/bin/conf/log.yml");
    apollo::Config::LoadFromYaml(root);

    std::cout << "============= after==============" << std::endl;
    std::cout << apollo::LoggerMgr::GetInstance()->toYamlString() << std::endl;
}

int main(int argc, char** argv) {
    /* test_yaml(); */
    /* test_config(); */
    /* test_class(); */
    test_log();

    apollo::Config::Visit([](apollo::ConfigVarBase::ptr var) {
        APOLLO_LOG_INFO(APOLLO_LOG_ROOT()) << "Name: " << var->getName()
                    << " Description: " << var->getDescription()
                    << " Typename: " << var->getTypeName()
                    << " Value: " << var->toString();
    });

    return 0;
}
