#pragma once

#include <memory>
#include <functional>
#include <shared_mutex>
#include <type_traits>
#include <compare>
#include <set>

#include "common/singleton.h"
#include "common/stream.h"
#include "common/types.h"

#define REGISTER_MODULE(T, name, version, filename) \
    static nemo::ModuleRegister<T> moduleRegister##T(name, version, filename)

namespace nemo {

/**
 * @brief 模块类
 */
class Module {
public:
    typedef std::shared_ptr<Module> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<Module> UniquePtr; ///< 智能指针定义

    /**
     * @brief 构造函数
     * @param[in] name 模块名
     * @param[in] version 模块版本
     * @param[in] filename 模块的文件路径
     */
    Module(StringArg name, StringArg version,
        StringArg filename);

    std::strong_ordering operator<=>(const Module& other) const {
        return name_ <=> other.name_;
    }

    /**
     * @brief 析构函数
     */
    virtual ~Module() = default;

    virtual bool onLoad();
    virtual bool onUnload();

    virtual bool onConnect(Stream* stream);
    virtual bool onDisconnect(Stream* stream);
    
    virtual bool onServerReady();
    virtual bool onServerUp();

    virtual String toString();

    const String& getName() const { return name_; }
    const String& getVersion() const { return version_; }
    const String& getFilename() const { return filename_; }

    void setFilename(StringArg filename) { filename_ = filename; }

private:
    String name_;      ///< 模块名
    String version_;   ///< 模块版本
    String filename_;  ///< 模块文件路径
};

struct ModulePtrLessCompare {
    bool operator()(const Module::UniquePtr& lhs, 
        const Module::UniquePtr& rhs) const {
        return *lhs < *rhs;
    }
};

/**
 * @brief 模块管理器类
 */
class ModuleManager : public Singleton<ModuleManager> {
public:
    /**
     * @brief 默认构造
     */
    ModuleManager(Token);

    /**
     * @brief 添加模块
     * @param[in] module 要添加的模块
     */
    void add(Module::UniquePtr&& module);

    /**
     * @brief 删除模块
     * @param[in] name 要删除的模块名字
     */
    void del(StringArg name);

    /**
     * @brief 删除所有模块
     */
    void clear();

    /**
     * @brief 通过名字得到模块
     * @param[in] name 模块名
     * @return[in] 对应模块的智能指针 
     */
    Module* get(StringArg name);

    /**
     * @brief 遍历所有模块
     * @param[in] cb 对模块执行的回调函数
     */
    void foreach(std::function<void(Module*)> cb);

private:
    typedef std::set<Module::UniquePtr> ModuleSet;

private:
    std::shared_mutex mutex_; ///< 读写锁
    ModuleSet modules_;       ///< 保存的模块
};

/**
 * @brief 模块注册类定义
 */
template<typename T>
class ModuleRegister {
static_assert(std::is_convertible_v<T, Module>, "type mismatch");
public:
    /**
     * @brief 构造函数
     */
    ModuleRegister(StringArg name, StringArg version,
            StringArg filename);
};

template<typename T>
ModuleRegister<T>::ModuleRegister(StringArg name, StringArg version,
        StringArg filename) {
    ModuleManager::GetInstance().add(std::make_unique<T>(name, version, filename));
}

} // namespace