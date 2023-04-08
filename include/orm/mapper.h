#pragma once

#include <unordered_map>
#include <shared_mutex>

#include <tinyxml2.h>
#include <yaml-cpp/yaml.h>

#include "common/types.h"
#include "common/singleton.h"
#include "common/lexical_cast.h"

namespace nemo {
namespace orm {

struct MapperConfig {
    String path;

    bool operator==(const MapperConfig& other) const {
        return path == other.path;
    }
};

class MapperManager;

class Mapper {
friend class MapperManager;
public:
    typedef std::unique_ptr<Mapper> UniquePtr;
    typedef std::shared_ptr<Mapper> SharedPtr;

public:
    Mapper(tinyxml2::XMLElement* element) noexcept;

    tinyxml2::XMLElement* getElement() const { return element_; }
    const String& getName() const { return name_; }
    const String& getQuery();

private:
    tinyxml2::XMLElement* element_;
    String name_;
    String query_;
};

class MapperManager : public Singleton<MapperManager> {
public:
    MapperManager(Token token);
    void load(StringArg path);
    Mapper* getMapper(const String& name);
    void addMapper(std::unique_ptr<Mapper>&& mapper);
    void addMapper(Mapper&& mapper);
    void eraseMapper(const String& name);

private:
    std::unordered_map<String, std::unique_ptr<Mapper>> mappers_;
    std::shared_mutex mutex_;
    tinyxml2::XMLDocument doc_;
};

} // naemspace orm

template<>
inline String LexicalCast<String, orm::MapperConfig>(const orm::MapperConfig& config) {
    YAML::Node node;
    node["path"] = config.path;
    return (std::stringstream() << node).str();
}

template<>
inline orm::MapperConfig LexicalCast<orm::MapperConfig, String>(const String& yamlStr) {
    YAML::Node node = YAML::Load(yamlStr);
    orm::MapperConfig config;
    config.path = node["path"].as<String>();
    return config;
}

} // namespace nemo