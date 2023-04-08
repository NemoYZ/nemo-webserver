#include <vector>
#include <iostream>

#include "db/db.h"
#include "log/log.h"
#include "common/config.h"

static nemo::Logger::SharedPtr rootLogger = NEMO_LOG_NAME("root");

void LoadDb();

int main(int argc, char** argv) {
    LoadDb();
    soci::session* s = nemo::db::DbManager::GetInstance().getSessionAny("mysql");
    assert(s);
    soci::rowset<> rows = (s->prepare << "select * from tb_player");

    for (auto iter = rows.begin(); iter != rows.end(); ++iter) {
        for (size_t i = 0; i < iter->size(); ++i) {
            if (iter->get_indicator(i) == soci::i_null) {
                std::cout << "null" << " ";
            } else {
                try {
                    switch (iter->get_properties(i).get_data_type())
                    {
                        case soci::data_type::dt_integer:
                            std::cout << iter->get<int>(i) << " ";
                            break;
                        case soci::data_type::dt_double:
                            std::cout << iter->get<double>(i) << " ";
                            break;
                        case soci::data_type::dt_long_long:
                            std::cout << iter->get<long long>(i) << " ";
                            break;
                        case soci::data_type::dt_string:
                            std::cout << iter->get<std::string>(i) << " ";
                            break;
                        default:
                            break;
                    }
                } catch (const std::exception& e) {
                    std::cout << "get except, e.what()=" << e.what() << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }

    return 0;
}

void LoadDb() {
    nemo::Config::LoadFromDir("/programs/nemo/config");
    static nemo::ConfigVar<std::vector<nemo::db::DbConfig>>* dbsConfigs
        = nemo::Config::Lookup("databases", std::vector<nemo::db::DbConfig>(), "db config");
    auto dbConfigs = dbsConfigs->getValue();
    for (auto& dbConfig : dbConfigs) {
        NEMO_LOG_DEBUG(rootLogger) << "connect parameter=" << dbConfig.toConnectParameter();
        nemo::db::DbManager::GetInstance().connect(dbConfig.name, dbConfig.toConnectParameter(), dbConfig.connections);
    }
}