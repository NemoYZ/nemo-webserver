#include <iostream>
#include <yaml-cpp/yaml.h>
#include "common/config.h"
#include "log/log.h"

using namespace nemo;

static ConfigVar<int>* gIntValueConfig =
    Config::Lookup("system.port", (int)8080, "system port");

static ConfigVar<float>* gFloatValueConfig =
    Config::Lookup("system.value", (float)10.2f, "system value");

static ConfigVar<std::vector<int>>* gIntVecValueConfig =
    Config::Lookup("system.int_vec", std::vector<int>{1,2}, "system int vec");

static ConfigVar<std::list<int>>* gIntListValueConfig =
    Config::Lookup("system.int_list", std::list<int>{1,2}, "system int list");

static ConfigVar<std::set<int>>* gIntSetValueConfig =
    Config::Lookup("system.int_set", std::set<int>{1,2}, "system int set");

static ConfigVar<std::unordered_set<int>>* gIntUsetValueConfig =
    Config::Lookup("system.int_uset", std::unordered_set<int>{1,2}, "system int uset");

static ConfigVar<std::map<std::string, int>>* gStrIntMapValueConfig =
    Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k",2}}, "system str int map");

static ConfigVar<std::unordered_map<std::string, int>>* gStrIntUmapValueConfig =
    Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k",2}}, "system str int map");

void PrintYaml(const YAML::Node& node, int level);
void TestYaml();
void TestConfig();
void TestLog();
void TestLoad();
void TestLog2();

Logger::SharedPtr gRootLogger = NEMO_LOG_ROOT();

int main(int argc, char* argv[]) {
    //TestYaml();
    //TestLog();
    TestLoad();
    //TestLog2();
    
    return 0;
}

void PrintYaml(const YAML::Node& node, int level) {
    if(node.IsScalar()) {
        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "The yaml node is scalar ";
        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << std::string(level * 4, ' ')
            << node.Scalar() << " - " << node.Type() << " - " << level;
    } 
    else if(node.IsNull()) {
        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "The yaml node is null";
        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << std::string(level * 4, ' ')
            << "NULL - " << node.Type() << " - " << level;
    } 
    else if(node.IsMap()) {

        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "The yaml node is map";
        for(auto iter = node.begin();
                iter != node.end(); ++iter) {
            NEMO_LOG_INFO(NEMO_LOG_ROOT()) << std::string(level * 4, ' ')
                    << iter->first << " - " << iter->second.Type() 
                    << " - " << level;
            PrintYaml(iter->second, level + 1);
        }
    } 
    else if(node.IsSequence()) {
        for(decltype(node.size()) i = 0; i < node.size(); ++i) 
        {
            NEMO_LOG_INFO(NEMO_LOG_ROOT()) << std::string(level * 4, ' ')
                << i << " - " << node[i].Type() << " - " << level;
            PrintYaml(node[i], level + 1);
        }
    }
}

void TestYaml() {
    YAML::Node root = YAML::LoadFile("/programs/nemo/bin/config/log.yaml");
    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << std::boolalpha << root["test"].IsDefined();
    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << root["logs"].IsDefined();
    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "root: " << root;
}

void TestConfig() {
    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "before: " 
        << gIntValueConfig->getValue();
    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "before: " 
        << gFloatValueConfig->toString();

#define XX(gVar, name, prefix) \
    { \
        auto& v = gVar->getValue(); \
        for(auto& i : v) { \
            NEMO_LOG_INFO(NEMO_LOG_ROOT()) << #prefix " " #name ": " << i; \
        } \
        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << #prefix " " #name " yaml: " << gVar->toString(); \
    }

#define XX_M(gVar, name, prefix) \
    { \
        auto& v = gVar->getValue(); \
        for(auto& i : v) { \
            NEMO_LOG_INFO(NEMO_LOG_ROOT()) << #prefix " " #name ": {" \
                    << i.first << " - " << i.second << "}"; \
        } \
        NEMO_LOG_INFO(NEMO_LOG_ROOT()) << #prefix " " #name " yaml: " << gVar->toString(); \
    }


    XX(gIntVecValueConfig, intVec, before);
    XX(gIntListValueConfig, intList, before);
    XX(gIntSetValueConfig, intSet, before);
    XX(gIntUsetValueConfig, intUset, before);
    XX_M(gStrIntMapValueConfig, strIntMap, before);
    XX_M(gStrIntUmapValueConfig, strIntUmap, before);

    YAML::Node root = YAML::LoadFile("/programs/nemo/bin/config/log.yml");
    nemo::Config::LoadFromYaml(root);

    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "after: " 
        << gIntValueConfig->getValue();
    NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "after: " 
        << gFloatValueConfig->toString();

    XX(gIntVecValueConfig, intVec, after);
    XX(gIntListValueConfig, intList, after);
    XX(gIntSetValueConfig, intSet, after);
    XX(gIntUsetValueConfig, intUset, after);
    XX_M(gStrIntMapValueConfig, strIntMap, after);
    XX_M(gStrIntUmapValueConfig, strIntUmap, after);
}

void TestLog() {
    Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");
    NEMO_LOG_INFO(systemLogger) << "hello system" << std::endl;
    std::cout << LoggerManager::GetInstance().toYamlString() << std::endl;
    std::cout << "==============" << std::endl;
    YAML::Node root = YAML::LoadFile("/programs/nemo/bin/config/log.yaml");
    Config::LoadFromYaml(root);
    std::cout << "=============" << std::endl;
    std::cout << LoggerManager::GetInstance().toYamlString() << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << root << std::endl;
    NEMO_LOG_INFO(systemLogger) << "hello system";

    systemLogger->setFormat("%d%m%n");
    NEMO_LOG_INFO(systemLogger) << "Hello System";
}

void TestLoad() {
    std::string path = "/programs/nemo/bin/config";
    Config::LoadFromDir(path);
    Config::Visit([](nemo::ConfigVarBase* configVarBase){
        std::cout << configVarBase->toString();
    });
}

void TestLog2() {
    Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");
    std::cout << "===============================" << std::endl;
    Config::LoadFromDir("/programs/nemo/bin/config");
    std::cout << LoggerManager::GetInstance().toYamlString() << std::endl;
    NEMO_LOG_DEBUG(systemLogger) << "Change the boss of this gym" << std::endl;
}