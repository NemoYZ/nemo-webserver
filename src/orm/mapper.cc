#include "orm/mapper.h"

#include <mutex>

#include "common/macro.h"
#include "log/log.h"

namespace nemo {
namespace orm {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

Mapper::Mapper(tinyxml2::XMLElement* element) noexcept :
    element_(element) {
    const char* name{nullptr};
    NEMO_ASSERT(element_->QueryAttribute("name", &name) == tinyxml2::XMLError::XML_SUCCESS);
    name_ = name;
}

const String& Mapper::getQuery() {
    if (!query_.empty()) {
        return query_;
    }
    tinyxml2::XMLElement* currElement = element_->FirstChildElement();
    while (currElement) {
        static constexpr std::string_view queryView = "query";
        if (::strncasecmp(currElement->Name(), queryView.data(), queryView.size()) == 0) {
            const char* text = currElement->GetText();
            if (text) {
                query_ = text;
            }
            break;
        }
        currElement = currElement->NextSiblingElement();
    }
    if (query_.empty()) {
        NEMO_LOG_WARN(systemLogger) << "mapper.name=" << name_ << " without query";
    }
    return query_;
}

MapperManager::MapperManager(Token token) {
}

void MapperManager::load(StringArg path) {
    if (doc_.LoadFile(path.data()) != tinyxml2::XMLError::XML_SUCCESS) {
        NEMO_LOG_ERROR(systemLogger) << "load mapper file, path=" << path 
            << " faild";
        return;
    }
    tinyxml2::XMLElement* child = doc_.FirstChildElement()->FirstChildElement();
    while (child) {
        constexpr static std::string_view mapperView = "mapper";
        NEMO_ASSERT(::strncasecmp(child->Name(), mapperView.data(), mapperView.size()) == 0);
        const char* name{nullptr};
        if (child->QueryAttribute("name", &name) != tinyxml2::XML_SUCCESS) {
            NEMO_LOG_ERROR(systemLogger) << "mapper without name, element name=" << child->Name();
        } else {
            Mapper mapper(child);
            mapper.name_ = name;
            std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
            mappers_.emplace(name, std::make_unique<Mapper>(mapper));
        }
        child = child->NextSiblingElement();
    }
}

Mapper* MapperManager::getMapper(const String& name) {
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    auto iter = mappers_.find(name);
    if (iter != mappers_.end()) {
        return iter->second.get();
    }
    return nullptr;
}

void MapperManager::addMapper(std::unique_ptr<Mapper>&& mapper) {
    if (mapper) {
        String mapperName = mapper->name_;
        std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
        mappers_.emplace(mapperName, std::move(mapper));
    }
}

void MapperManager::addMapper(Mapper&& mapper) {
    String mapperName = mapper.name_;
    std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
    mappers_.emplace(mapperName, std::make_unique<Mapper>(std::move(mapper)));
}

void MapperManager::eraseMapper(const String& name) {
    std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
    mappers_.erase(name);
}

} // namespace orm
} // namespace nemo