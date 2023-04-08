#pragma once

#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <yaml-cpp/yaml.h>

#include "common/lexical_cast.h"
#include "common/types.h"

namespace nemo {

/**
 * @brief yaml字符串转化为序列容器的通用接口, str to Sequence container
 */
template<typename Container>
static Container Str2SequenceContainer(const String& yamlStr) {
    using T = typename Container::value_type;
    
    YAML::Node node = YAML::Load(yamlStr);
    Container container;
    std::stringstream ss;
    for(size_t i = 0; i < node.size(); ++i) {
        ss.str("");
        ss << node[i];
        container.push_back(LexicalCast<T>(ss.str()));
    }
    return container;
}

/**
 * @brief yaml字符串转化为set容器的通用接口, str to set container
 */
template<typename Container>
static Container Str2SetContainer(const String& yamlStr) {
    using T = typename Container::value_type;
    YAML::Node node = YAML::Load(yamlStr);
    Container container;
    std::stringstream ss;
    for(size_t i = 0; i < node.size(); ++i) {
        ss.str("");
        ss << node[i];
        container.insert(LexicalCast<T>(ss.str()));
    }
    return container;
}

/**
 * @brief yaml字符串转化为map容器的通用接口, str to map container
 */
template<typename Container>
static Container Str2MapContainer(const String& yamlStr) {
    using T = typename Container::mapped_type;
    
    YAML::Node node = YAML::Load(yamlStr);
    Container container;
    std::stringstream ss;
    for(auto iter = node.begin(); 
        iter != node.end(); ++iter) {
        ss.str("");
        ss << iter->second;
        container[iter->first.Scalar()] = LexicalCast<T>(ss.str());
    }
    return container;
}

/**
 * @brief 容器转化为yaml字符串的通用接口, container to str
 */
template<typename Container>
static String Container2Str(const Container& container) {
    YAML::Node node;
    using T = typename Container::value_type;
    for(auto& elem : container) {
        node.push_back(YAML::Load(LexicalCast<String, T>(elem)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

/**
 * @brief map容器转化为yaml字符串的通用接口
 */
template<typename Container>
static String MapContainer2Str(const Container& container) {
    YAML::Node node;
    using T = typename Container::mapped_type;
    for(auto& elem : container) {
        node.push_back(YAML::Load(LexicalCast<String, T>(elem.second)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

template<typename Target, typename Source>
class YamlCast {
public:
    Target operator()(const Source& source) {
        return LexicalCast<Target>(source);
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::vector<T>)
 */
template<typename T>
class YamlCast<std::vector<T>, String> {
public:
    std::vector<T> operator()(const String& yamlStr) {
        return Str2SequenceContainer<std::vector<T>>(yamlStr);
    }
};

/**
 * @brief 类型转换模板类偏特化(std::vector<T> 转换成 YAML String)
 */
template<typename T>  
class YamlCast<String, std::vector<T>> {
public:
    String operator()(const std::vector<T>& vec) {
        return Container2Str(vec);
    }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::list<T>)
 */
template<typename T>
class YamlCast<std::list<T>, String> {
public:
    std::list<T> operator()(const String& yamlStr) {
        return Str2SequenceContainer<std::list<T>>(yamlStr);
    }
};

/**
 * @brief 类型转换模板类片特化(std::list<T> 转换成 YAML String)
 */
template<typename T>
class YamlCast<String, std::list<T>> {
public:
    String operator()(const std::list<T>& lst) {
        return Container2Str(lst);
    }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::set<T>)
 */
template<typename T>
class YamlCast<std::set<T>, String> {
public:
    std::set<T> operator()(const String& yamlStr) {
        return Str2SetContainer<std::set<T>>(yamlStr);
    }
};

/**
 * @brief 类型转换模板类片特化(std::set<T> 转换成 YAML String)
 */
template<typename T>
class YamlCast<String, std::set<T>> {
public:
    String operator()(const std::set<T>& st) {
        return Container2Str(st);
    }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::unordered_set<T>)
 */
template<typename T>
class YamlCast<std::unordered_set<T>, String> {
public:
    std::unordered_set<T> operator()(const String& yamlStr) {
        return Str2SetContainer<std::unordered_set<T>>(yamlStr);
    }
};

/**
 * @brief 类型转换模板类片特化(std::unordered_set<T> 转换成 YAML String)
 */
template<typename T>
class YamlCast<String, std::unordered_set<T>> {
public:
    String operator()(const std::unordered_set<T>& uset) {
        return Container2Str(uset);
    }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::map<String, T>)
 */
template<typename T>
class YamlCast<std::map<String, T>, String> {
public:
    std::map<String, T> operator()(const String& yamlStr) {
        return Str2MapContainer<std::map<String, T>>(yamlStr);
    }
};

/**
 * @brief 类型转换模板类片特化(std::map<String, T> 转换成 YAML String)
 */
template<typename T>
class YamlCast<String, std::map<String, T>> {
public:
    String operator()(const std::map<String, T>& mp) {
        return MapContainer2Str(mp);
    }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::unordered_map<String, T>)
 */
template<typename T>
class YamlCast<std::unordered_map<String, T>, String> {
public:
    std::unordered_map<String, T> operator()(const String& yamlStr) {
        return Str2MapContainer<std::unordered_map<String, T>>(yamlStr);
    }
};

/**
 * @brief 类型转换模板类片特化(std::unordered_map<String, T> 转换成 YAML String)
 */
template<typename T>
class YamlCast<String, std::unordered_map<String, T>> {
public:
    String operator()(const std::unordered_map<String, T>& umap) {
        return MapContainer2Str(umap);
    }
};

} //namespace nemo
