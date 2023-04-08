#include "common/config.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "log/log.h"

namespace nemo {

static nemo::Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

static void listAllMember(const String& prefix,
                          const YAML::Node& node,
                          std::list<std::pair<String, const YAML::Node>>& output) {
    std::regex e("[^a-z0-9._]");
    if (std::regex_search(prefix, e)) {
        NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "Config invalid name: "
                                        << prefix 
                                        << " : "
                                        << node;
        return;
    }

    output.push_back({prefix, node});
    if (node.IsMap()) {
        for (auto iter = node.begin();
                iter != node.end(); ++iter) {
            listAllMember(prefix.empty() ? iter->first.Scalar() : 
                        prefix + "." + iter->first.Scalar(),
                        iter ->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node& root) {
    std::list<std::pair<String, const YAML::Node>> allNodes;
    listAllMember("", root, allNodes);

    for(auto& node : allNodes) {
        String key = std::move(node.first);
        if(key.empty()) {
            continue;
        }

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase* configVarBase = LookupBase(key);

        if(configVarBase) {
            if(node.second.IsScalar()) {
                configVarBase->fromString(node.second.Scalar());
            } else {
                std::stringstream ss;
                ss << node.second;
                configVarBase->fromString(ss.str());
            }
        }
    }
}

void Config::LoadFromDir(const String& path) {
    std::vector<String> files;
    ListFiles(files, path, {"yml", "yaml"});
    for (String& file : files) {
        LoadFromFile(file);
    }
}


void Config::LoadFromFile(const String& path) {
    static std::unordered_map<String, uint64_t> fileLastModifyTime;
    static std::mutex fileModifyTimeMutex;
    struct stat st;

    ::stat(path.c_str(), &st);
    {
        std::lock_guard<std::mutex> lock(fileModifyTimeMutex);
        if (fileLastModifyTime[path] == static_cast<uint64_t>(st.st_mtime)) {
            return;
        }
        fileLastModifyTime[path] = st.st_mtime;
    }
    try {
        YAML::Node node = YAML::LoadFile(path);
        LoadFromYaml(node);
    } catch (...) {
        NEMO_LOG_ERROR(systemLogger) << "LoadConfigFile file="
                            << path << " failed";
    }
}

void Config::Visit(std::function<void(ConfigVarBase*)> cb) {
    std::shared_lock<std::shared_mutex> sharedLock(GetMutex());
    ConfigVarMap& m = GetDatas();

    for(auto iter = m.begin();
            iter != m.end(); ++iter) {
        cb(iter->second.get());
    }
}

}   //namespace nemo