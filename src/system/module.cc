#include "system/module.h"

#include "common/config.h"

namespace nemo {

static ConfigVar<std::string>* modulePath
    = Config::Lookup("module.path", String("module"), "module path");

Module::Module(StringArg name,
        StringArg version,
        StringArg filename) :
    name_(name),
    version_(version),
    filename_(filename) {
}

std::string Module::toString() {
    std::stringstream ss;
    ss << "Module name=" << getName()
       << " version=" << getVersion()
       << " filename=" << getFilename()
       << "\n";
    return ss.str();
}

bool Module::onLoad() {
    return true;
}

bool Module::onUnload() {
    return true;
}

bool Module::onConnect(Stream* stream) {
    return true;
}

bool Module::onDisconnect(Stream* stream) {
    return true;
}

bool Module::onServerReady() {
    return true;
}

bool Module::onServerUp() {
    return true;
}

Module* ModuleManager::get(StringArg name) {
    Module::UniquePtr tmpModule = std::make_unique<Module>(name, "", "");
    std::shared_lock<std::shared_mutex> sharedLock(mutex_);
    auto iter = modules_.find(tmpModule);
    return iter == modules_.end() ? nullptr : iter->get();
}

ModuleManager::ModuleManager(Token) {
}

void ModuleManager::add(Module::UniquePtr&& module) {
    del(module->getName());
    std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
    modules_.emplace(std::move(module));
}

void ModuleManager::del(StringArg name) {
    Module::UniquePtr tmpModule = std::make_unique<Module>(name, "", "");
    std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
    auto iter = modules_.find(tmpModule);
    if(iter == modules_.end()) {
        return;
    }
    (*iter)->onUnload();
    modules_.erase(iter);
}

void ModuleManager::clear() {
    std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
    for (auto iter = modules_.begin();
            iter != modules_.end(); ++iter) {
        (*iter)->onUnload();
    }
    modules_.clear();
}

void ModuleManager::foreach(std::function<void(Module*)> cb) {
    for (auto iter = modules_.begin();
            iter != modules_.end(); ++iter) {
        cb(iter->get());
    }
}

} // namespace nemo