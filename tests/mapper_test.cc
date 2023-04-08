#include "orm/mapper.h"
#include "log/log.h"
#include "common/config.h"

static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void LoadMappers();

int main(int argc, char** argv) {
    LoadMappers();
    nemo::orm::Mapper* mapper = nemo::orm::MapperManager::GetInstance().getMapper("GuildMapper");
    if (nullptr == mapper) {
        NEMO_LOG_ERROR(rootLogger) << "get mapper faild";
    } else {
        NEMO_LOG_DEBUG(rootLogger) << "mapper.name=" << mapper->getName();
        NEMO_LOG_DEBUG(rootLogger) << "mapper.query=" << mapper->getQuery();
    }

    return 0;
}

void LoadMappers() {
    nemo::Config::LoadFromFile("../config/mapper.yaml");
    static nemo::ConfigVar<std::vector<nemo::orm::MapperConfig>>* mappersConfigs
        = nemo::Config::Lookup("mappers", std::vector<nemo::orm::MapperConfig>(), "mapper config");
    auto mapperConfigs = mappersConfigs->getValue();
    for (auto& mapperConfig : mapperConfigs) {
        NEMO_LOG_DEBUG(rootLogger) << "path=" << mapperConfig.path;
        nemo::orm::MapperManager::GetInstance().load(mapperConfig.path);
    }
}