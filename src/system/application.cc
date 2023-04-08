#include "system/application.h"

#include <vector>
#include <iostream>

#include "system/daemon.h"
#include "system/module.h"
#include "system/env.h"
#include "log/log.h"
#include "common/config.h"
#include "net/http/http_server.h"
#include "coroutine/coroutine.h"
#include "db/db.h"
#include "orm/mapper.h"

namespace nemo {

static Logger::SharedPtr systemLogger = NEMO_LOG_NAME("system");

static ConfigVar<std::vector<net::ServerConfig>>* serversConfigs
    = Config::Lookup("servers", std::vector<net::ServerConfig>(), "servers config");

static nemo::ConfigVar<std::vector<nemo::db::DbConfig>>* dbsConfigs
    = nemo::Config::Lookup("databases", std::vector<nemo::db::DbConfig>(), "db config");

static ConfigVar<std::vector<orm::MapperConfig>>* ormsConfigs
    = Config::Lookup("mappers", std::vector<orm::MapperConfig>(), "mappers configs");

Application::Application(Token) :
    isDaemon_(false) {
}

void Application::runTask() {
    ModuleManager::GetInstance().foreach([](Module* module){
        if(!module->onLoad()) {
            NEMO_LOG_ERROR(systemLogger) << "module name=" << module->getName() 
                << " version=" << module->getVersion()
                << " filename=" << module ->getFilename()
                << " load fail";
            ::exit(-1);
        }
    });

    // 加载服务器
    auto serverConfigs = serversConfigs->getValue();
    std::vector<net::TcpServer::UniquePtr> servers;
    for(auto& serverConfig : serverConfigs) {
        NEMO_LOG_DEBUG(systemLogger) << "\n" << LexicalCast<String, net::ServerConfig>(serverConfig);
        std::vector<net::Address::UniquePtr> addresses;
        for(auto& address : serverConfig.addresses) {
            String::size_type pos = address.find_last_of(":");
            if(pos == std::string::npos) {
                addresses.emplace_back(std::make_unique<net::UnixAddress>(address));
                continue;
            }
            int32_t port = ::atoi(address.substr(pos + 1).data());
            //127.0.0.1
            net::Address::UniquePtr addr = net::IpAddress::Create(address.substr(0, pos).data(), port);
            if(addr) {
                addresses.emplace_back(std::move(addr));
                continue;
            }
            std::vector<std::pair<net::Address::UniquePtr, uint32_t>> interfaceAddress;
            net::Address::GetInterfaceAddresses(address.substr(0, pos), 
                std::back_insert_iterator<decltype(interfaceAddress)>(interfaceAddress));
            if (!interfaceAddress.empty()) {
                for(auto iter = interfaceAddress.begin();
                        iter != interfaceAddress.end(); ++iter) {
                    net::IpAddress* ipaddr = dynamic_cast<net::IpAddress*>(iter->first.get());
                    if(ipaddr) {
                        ipaddr->setPort(::atoi(address.substr(pos + 1).c_str()));
                    }
                    addresses.emplace_back(std::move(iter->first));
                }
                continue;
            }

            net::Address::UniquePtr aaddr = net::Address::LookupAny(address);
            if(aaddr) {
                addresses.emplace_back(std::move(aaddr));
                continue;
            }
            NEMO_LOG_ERROR(systemLogger) << "invalid address: " << address;
            exit(0);
        }

        net::TcpServer::UniquePtr server;
        if(serverConfig.type == "http") {
            server = std::make_unique<net::http::HttpServer>(serverConfig.keepAlive);
        }
        //and so on(https, rpc, echo)

        if(!serverConfig.name.empty()) {
            server->setName(serverConfig.name);
        }
        std::vector<net::Address*> fails;
        if(!server->bind(addresses, fails, serverConfig.ssl)) {
            ::exit(0);
        }
        if(serverConfig.ssl) {
            if(!server->loadCertificates(serverConfig.certFile, serverConfig.keyFile)) {
                NEMO_LOG_ERROR(systemLogger) << "loadCertificates fail, cert_file="
                    << serverConfig.certFile << " key_file=" << serverConfig.keyFile;
            }
        }
        server->setConfig(serverConfig);
        String serverName = server->getName();
        servers_.emplace(serverName, std::move(server));
    }

    ModuleManager::GetInstance().foreach([](Module* module){
        module->onServerReady();
    });
    std::cout << "start server: \n";
    for (auto& [serverName, server] : servers_) {
        std::cout << server->toString("");
        server->start();
    }

    ModuleManager::GetInstance().foreach([](Module* module){
        module->onServerUp();
    });
}

int Application::main(int argc, char** argv) {
    coroutine_async [this](){
        this->runTask();
    };
    coroutine_start;

    return 0;
}

void Application::loadConfig(const String& configFile) {
    Config::LoadFromDir(configFile);
}

int Application::run(int argc, char** argv) {
    static bool called = false;
    if (called) {
        return 0;
    }
    called = true;

    YAML::Node node;
    String loadPath = Env::GetInstance().getWorkDir() + "/../resource/application.yaml";
    try {
        node = YAML::LoadFile(loadPath);
        if (node["application"]["config"]["daemon"].IsDefined() && node["application"]["config"]["daemon"].as<bool>()) {
            isDaemon_ = true;
            std::cout << "run as daemon" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "load faild, path=" << loadPath 
            << " e.what()=" << e.what()
            << std::endl;
    }
    
    try {
        loadConfig(node["application"]["config"]["path"].as<String>());
    } catch (const std::exception& e) {
        std::cout << "load application.yaml faild, " << e.what() << std::endl;
        return -1;
    } catch(...) {
        std::cout << "load application.yaml faild, unknow error" << std::endl;   
        return -1;
    }

    //加载数据库
    auto dbConfigs = dbsConfigs->getValue();
    for (auto& dbConfig : dbConfigs) {
        db::DbManager::GetInstance().connect(dbConfig.name, dbConfig.toConnectParameter(), dbConfig.connections);
        std::cout << "connect finish" << std::endl;
    }

    // 加载mappers
    auto ormConfigs = ormsConfigs->getValue();
    for (auto& ormConfig : ormConfigs) {
        orm::MapperManager::GetInstance().load(ormConfig.path);
    }

    if (isDaemon_) {
        return CreateDaemon([this](int argc, char** argv)->int{
            return this->main(argc, argv);
        }, argc, argv); 
    } else {
        return main(argc, argv);
    }
}

net::TcpServer* Application::getServer(const String& name) {
    auto iter = servers_.find(name);
    return iter == servers_.end() ? nullptr : iter->second.get();
}

} // namespace nemo