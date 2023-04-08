/**
 * yaml-cpp介绍
 * ● yaml基本语法介绍
 *      yaml的三种数据类型：键值对、数组、纯量(单个的，不可再分的量)
 *      键值对(用冒号表示, key: value)
 *      比如  Person: 
 *              name: 
 *                Van
 *      数组(以-开头)
 *      比如 Person: 
 *             -
 *               name: Van
 *             -
 *               name: bili
 *      纯量(比如上述的Van, bili这两个字符串)
 * ● yaml-cpp介绍
 * https://blog.csdn.net/briblue/article/details/89515470
 * 
 */
#pragma once

#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility> //std::pair
#include <regex>
#include <mutex>
#include <shared_mutex>
#include <functional>

#include <yaml-cpp/yaml.h>

#include "log/log.h"
#include "common/yaml_cast.h"
#include "util/util.h"
#include "common/types.h"

namespace nemo {

/**
 * @brief 配置变量的基类
 */
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> SharedPtr; ///< 智能指针定义
    typedef std::unique_ptr<ConfigVarBase> UniquePtr; ///< 智能指针定义

    /**
     * @brief 构造函数
     * @param[in] name 配置参数名称[0-9a-z_.]
     * @param[in] description 配置参数描述
     */
    ConfigVarBase(StringArg name, StringArg description = "") :
            name_(name.data(), name.size()),
            description_(description) {
        std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);
    }

    /**
     * @brief 析构函数
     */
    virtual ~ConfigVarBase() = default;
    
    /**
     * @brief 返回配置参数名称
     */
    const String& getName() const { return name_; }

    /**
     * @brief 返回配置参数的描述
     */
    const String& getDescription() const { return description_; }

    /**
     * @brief 转成字符串
     */
    virtual String toString() = 0;

    /**
     * @brief 从字符串初始化值
     */
    virtual bool fromString(const String& str) = 0;

    /**
     * @brief 返回配置参数值的类型名称
     */
    virtual String getTypeName() const = 0;

protected:
    String name_;         ///< 配置参数的名称
    String description_;  ///< 配置参数的描述
};

/**
 * @brief 配置参数模板子类,保存对应类型的参数值
 * @details T 参数的具体类型
 *          FromStr 从std::string转换成T类型的仿函数
 *          ToStr 从T转换成std::string的仿函数
 *          String 为YAML格式的字符串
 */
template<typename T, typename FromStr = YamlCast<T, String>, 
         typename ToStr = YamlCast<String, T>>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> SharedPtr;                               ///< 智能指针定义
    typedef std::unique_ptr<ConfigVar> UniquePtr;                               ///< 智能指针定义
    typedef std::function<void(const T& oldValue, const T& newVal)> OnChangeCb; ///< 回调函数定义

    /**
     * @brief 通过参数名,参数值,描述构造ConfigVar
     * @param[in] name 参数名称有效字符为[0-9a-z_.]
     * @param[in] defaultValue 参数的默认值
     * @param[in] description 参数的描述
     */
    ConfigVar(const String& name,
              const T& defaultValue,
              const String& description = "") :
        ConfigVarBase(name, description),
        value_(defaultValue) {
    }

    /**
     * @brief 将参数值转换成YAML String
     * @exception 当转换失败抛出异常
     * @return 转化之后的yaml字符串
     */
    String toString() override {
        try {
            std::shared_lock<std::shared_mutex> sharedLock(mutex_);
            return ToStr()(value_);
        } catch (std::exception& e) {
            NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "ConfigVar::toString exception"
                                              << e.what()
                                              << " convert: "
                                              << getTypeName()
                                              << " to string";
        }
        return "";
    }

    /**
     * @brief 从YAML String 转成参数的值
     * @exception 当转换失败抛出异常
     * @return 
     *      @retval true 转化成功
     *      @retval false 转化失败
     */
    bool fromString(const String& str) override {
        try {
            setValue(FromStr()(str));
        } catch (std::exception& e) {
            NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "ConfigVar::toString exception"
                                            << e.what()
                                            << " convert: string to "
                                            << getTypeName()
                                            << " - "
                                            << str;
            return false;
        }
        return true;
    }

    /**
     * @brief 获取当前参数的值
     * @return 配置的值
     */
    const T& getValue() { 
        std::shared_lock<std::shared_mutex> sharedLock(mutex_);
        return value_;
    }

    /**
     * @brief 设置当前参数的值
     * @details 如果参数的值有发生变化,则通知对应的注册回调函数
     */
    void setValue(const T& val) {
        {
            std::shared_lock<std::shared_mutex> sharedLock(mutex_);
            if (val == value_) {
                return;
            }
             
            for (auto& cb : cbs_) { //值被改变了，调用回调函数
                cb.second(value_, val);
            }
            //离开作用域,锁释放
        }
        std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
        value_ = val;
    }

    /**
     * @brief 设置当前参数的值
     * @param[in] rval 参数类型的右值
     */
    void setValue(T&& rval) { 
        {
            std::shared_lock<std::shared_mutex> sharedLock(mutex_);
            if (rval == value_) {
                return;
            }
            for (auto& cb : cbs_) {    //值被改变了，调用回调函数
                cb.second(value_, rval);
            }
            //离开作用域,锁释放
        }
        std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
        value_ = std::move(rval); 
    }

    /**
     * @brief 返回参数值的类型名称(typeinfo)
     */
    String getTypeName() const override { return Demangle<T>(); }

    /**
     * @brief 添加变化回调函数
     * @return 返回该回调函数对应的唯一id,用于删除回调
     */
    uint64_t addListener(OnChangeCb cb) {
        static uint64_t sFunId = 0; //我们自己生成一个key，保证不重复
        std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
        cbs_.emplace(sFunId++, cb);
        return sFunId;              //将生成的key返回给用户
    }

    /**
     * @brief 删除回调函数
     * @param[in] key 回调函数的唯一id
     */
    void delListener(uint64_t key) {
        std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
        cbs_.erase(key);
    }

    /**
     * @brief 获取回调函数
     * @param[in] key 回调函数的唯一id
     * @return 如果存在返回对应的回调函数,否则返回nullptr
     */
    OnChangeCb getListener(uint64_t key) {
        std::shared_lock<std::shared_mutex> sharedLock(mutex_);
        auto iter = cbs_.find(key);
        return iter == cbs_.end() ? nullptr : iter->second;
    }

    /**
     * @brief 清理所有的回调函数
     */
    void clearListener() {
        std::unique_lock<std::shared_mutex> uniqueLock(mutex_);
        cbs_.clear();
    }

private:
    std::shared_mutex mutex_;                             ///< mutex
    T value_;                                       ///< 配置的值
    std::unordered_map<uint64_t, OnChangeCb> cbs_;  ///< 变更回调函数组, uint64_t key,要求唯一，一般可以用hash
};

/**
 * @brief ConfigVar的管理类
 * @details 提供便捷的方法创建/访问ConfigVar
 */
class Config {
public:
    typedef std::unordered_map<String, ConfigVarBase::UniquePtr> ConfigVarMap; ///< map类型

    /**
     * @brief 获取/创建对应参数名的配置参数
     * @param[in] name 配置参数名称
     * @param[in] defaultValue 参数默认值
     * @param[in] description 参数描述
     * @details 获取参数名为name的配置参数,如果存在直接返回
     *          如果不存在,创建参数配置并用defaultValue赋值
     * @return 返回对应的配置参数,如果参数名存在但是类型不匹配则返回nullptr
     * @exception 如果参数名包含非法字符[^0-9a-z_.] 抛出异常 std::invalid_argument
     */
    template<typename T>
    static ConfigVar<T>* Lookup(const String& name,
            const T& defaultValue, const String& description = "") {
        std::shared_lock<std::shared_mutex> sharedLock(GetMutex());
        auto iter = GetDatas().find(name);
        if (iter != GetDatas().end()) {
            ConfigVar<T>* tmp = down_cast<ConfigVar<T>*>(iter->second.get());
            if(tmp) {
                NEMO_LOG_INFO(NEMO_LOG_ROOT()) << "Lookup name=" 
                                               << name 
                                               << " exists";
                return tmp;
            } else {
                NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "Lookup name=" 
                                                << name
                                                << " exists but type not "
                                                << Demangle<T>()
                                                << " real_type=" 
                                                << iter->second->getTypeName()
                                                << " " 
                                                << iter->second->toString();
                return nullptr;
            }
        }
        sharedLock.unlock();
        
        std::regex e("[^a-z0-9._]");        
        
        if(std::regex_search(name, e)) {
            NEMO_LOG_ERROR(NEMO_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        ConfigVar<T>* configVar = new ConfigVar<T>(name, defaultValue, description);
        std::unique_lock<std::shared_mutex> uniqueLock(GetMutex());
        GetDatas().emplace(name, typename ConfigVar<T>::UniquePtr(configVar));
        return configVar;
    }

    /**
     * @brief 查找配置参数
     * @param[in] name 配置参数名称
     * @return 返回配置参数名为name的配置参数
     */
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const String& name) {
        std::shared_lock<std::shared_mutex> sharedLock(GetMutex());
        auto iter = GetDatas().find(name);
        if(GetDatas().end() == iter) {
            return nullptr;
        }
        return down_cast<ConfigVar<T>*>(iter->second.get());
    }

    /**
     * @brief 使用YAML::Node初始化配置模块
     */
    static void LoadFromYaml(const YAML::Node& root);

    /**
     * @brief 加载path文件夹里面的配置文件
     * @param[in] path 目录名
     */
    static void LoadFromDir(const String& path);

    /**
     * @brief 加载path所指定的配置文件
     * @param[in] path 文件名
     */
    static void LoadFromFile(const String& path);

    /**
     * @brief 查找配置参数,返回配置参数的基类
     * @param[in] name 配置参数名称
     */
    static ConfigVarBase* LookupBase(const String& name) {
        std::shared_lock<std::shared_mutex> sharedLock(GetMutex());
        auto iter = GetDatas().find(name);
        return GetDatas().end() == iter ? nullptr : iter->second.get();
    }

    /**
     * @brief 遍历配置模块里面所有配置项
     * @param[in] cb 配置项回调函数
     */
    static void Visit(std::function<void(ConfigVarBase*)> cb);

private:
    static ConfigVarMap& GetDatas() {
        /**
         * 这样通过函数返回我们的静态变量就几个好处
		 * 1.可以保证在使用前就被初始化
		 * 2.可以在其他文件中使用我们的静态变量
		 * 3.可以延迟初始化，只有在我们第一次使用到该静态变量时才会初始化
         */
        static ConfigVarMap sDatas;
        return sDatas;
    }

    /**
     * @brief 配置项的RWMutex
     */
    static std::shared_mutex& GetMutex() {
        static std::shared_mutex sharedMutex;
        return sharedMutex;
    }
};


}   //namespace nemo

