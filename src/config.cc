#include "config.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


namespace apollo {

static apollo::Logger::ptr g_logger = APOLLO_LOG_NAME("system");    // 生成一个名为system的全局logger

// 查找配置项
ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}

/* 解析出所有的yaml node */
static void ListAllMember(const std::string& prefix,
                        const YAML::Node& node,
                        std::list<std::pair<std::string, const YAML::Node> >& output) {
    if(prefix.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._0123456789")   // 默认以字母、'.'、下划线数字开头
            != std::string::npos) {
        APOLLO_LOG_ERROR(g_logger) << "CONFIG INVALID NAME " << prefix << " : " << node;
        return;
    }
    output.push_back(std::make_pair(prefix, node));
    if(node.IsMap()) {
        for(auto it = node.begin();
                it != node.end(); ++it) {
            ListAllMember(prefix.empty() ? it->first.Scalar()
                    : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

/* 从文件中加载yaml配置，会复写已存在配置项的参数 */
void Config::LoadFromYaml(const YAML::Node& root) {
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);

    for(auto& i : all_nodes) {
        std::string key = i.first;
        if(key.empty()) continue;

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);

        if(var) {
            // 新值覆盖旧值
            if(i.second.IsScalar()) {
                var->fromString(i.second.Scalar()); 
            } else {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}

/* 加载配置模块里面的所有配置项 */
void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
    RWMutexType::ReadLock lock(GetMutex());

    auto m = GetDatas();
    for(auto it = m.begin(); 
            it != m.end(); it++) {
        cb(it->second);
    }
}

}
// namespace apollo
