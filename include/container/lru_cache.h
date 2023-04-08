#pragma once

#include <unordered_map>
#include <functional>

#include "list.h"
#include "construct.h"

namespace nemo {

template<typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
class LruCache {
private:
    typedef DoubleLinkedNodeBase NodeBaseType;
    typedef std::unordered_map<Key, NodeBaseType*, Hash, Equal> MapType;
    typedef DoubleLinkedNode<std::pair<typename MapType::iterator, Value>> NodeType;

public:
    typedef typename MapType::size_type size_type;
    typedef Key                         key_type;
    typedef Value                       value_type;

public:
    explicit LruCache(size_type limit, size_type buckets = 16, const Hash& hasher = Hash(),
        const Equal& equaler = Equal());
    ~LruCache();

    bool get(const key_type& key, value_type& v);
    void put(const key_type& key, const value_type& v);
    void put(const key_type& key, value_type&& v);
    void put(key_type&& key, const value_type& v);
    void put(key_type&& key, value_type&& v);

    size_type size() const { return map_.size(); }
    size_type limit() const { return limit_; }

    void clear();

private:
    NodeBaseType* allocateNodeBase() {
        NodeBaseType* newNode = static_cast<NodeBaseType*>(::operator new(sizeof(NodeBaseType)));
        newNode->next = newNode->prev = newNode;
        return newNode;
    }
    NodeType* allocateNode() {
        NodeType* newNode = static_cast<NodeType*>(::operator new(sizeof(NodeType)));
        return newNode;
    }
    void destoryNode(NodeType* node) {
        destory(&(node->value));
        ::operator delete(node);
    }
    void destoryNodeBase(NodeBaseType* node) {
        ::operator delete(node);
    }
    void moveToHeader(NodeType* node);
    void clearWithoutLink();

private:
    NodeBaseType* header_{allocateNodeBase()};
    size_type limit_;
    MapType map_;
};

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::moveToHeader(NodeType* node) {
    UnlinkSelfUnckecked(node);
    LinkAfter(header_, node);
}

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::clearWithoutLink() {
    map_.clear();
    NodeBaseType* currNode = header_->next;
    while (currNode != header_) {
        NodeType* node = static_cast<NodeType*>(currNode);
        currNode = currNode->next;
        destoryNode(node);
    }
}

template<typename Key, typename Value, typename Hash, typename Equal>
LruCache<Key, Value, Hash, Equal>::LruCache(size_type limit, size_type buckets, 
    const Hash& hasher, const Equal& equaler) :
    limit_(limit),
    map_(buckets, hasher, equaler) {
}

template<typename Key, typename Value, typename Hash, typename Equal>
LruCache<Key, Value, Hash, Equal>::~LruCache() {
    clearWithoutLink();
    destoryNodeBase(header_);
}

template<typename Key, typename Value, typename Hash, typename Equal>
bool LruCache<Key, Value, Hash, Equal>::get(const key_type& key, value_type& v) {
    typename MapType::iterator iter = map_.find(key);
    if (iter != map_.end()) {
        NodeType* node = static_cast<NodeType*>(iter->second);
        v = node->value.second;
        moveToHeader(node);
        return true;
    }
    return false;
}

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::put(const key_type& key, const value_type& v) {
    typename MapType::iterator iter = map_.find(key);
    if (iter != map_.end()) {
        NodeType* node = static_cast<NodeType*>(iter->second);
        node->value.second = v;
        moveToHeader(node);
    } else {
        // 不必考虑自定义一个内存池来复用节点
        // 一个好的malloc肯定会考虑到相同大小内存块的频繁申请和释放这种情况
        if (map_.size() >= limit_) {
            NodeType* node = static_cast<NodeType*>(UnlinkBefore(header_));
            map_.erase(node->value.first);
            destoryNode(node);
        }
        NodeType* newNode = allocateNode();
        auto myPair = map_.emplace(key, newNode);
        construct(&newNode->value, myPair.first, v);
        LinkBefore(header_, newNode);
    }
}

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::put(const key_type& key, value_type&& v) {
    typename MapType::iterator iter = map_.find(key);
    if (iter != map_.end()) {
        NodeType* node = static_cast<NodeType*>(iter->second);
        node->value.second = std::move(v);
        moveToHeader(node);
    } else {
        if (map_.size() >= limit_) {
            NodeType* node = static_cast<NodeType*>(UnlinkBefore(header_));
            map_.erase(node->value.first);
            destoryNode(node);
        }
        NodeType* newNode = allocateNode();
        auto myPair = map_.emplace(key, newNode);
        construct(&newNode->value, myPair.first, std::move(v));
        LinkBefore(header_, newNode);
    }
}

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::put(key_type&& key, const value_type& v) {
    typename MapType::iterator iter = map_.find(key);
    if (iter != map_.end()) {
        NodeType* node = static_cast<NodeType*>(iter->second);
        node->value.second = v;
        moveToHeader(node);
    } else {
        if (map_.size() >= limit_) {
            NodeType* node = static_cast<NodeType*>(UnlinkBefore(header_));
            map_.erase(node->value.first);
            destoryNode(node);
        }
        NodeType* newNode = allocateNode();
        auto myPair = map_.emplace(std::move(key), newNode);
        construct(&newNode->value, myPair.first, std::move(v));
        LinkBefore(header_, newNode);
    }
}

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::put(key_type&& key, value_type&& v) {
    typename MapType::iterator iter = map_.find(key);
    if (iter != map_.end()) {
        NodeType* node = static_cast<NodeType*>(iter->second);
        node->value.second = std::move(v);
        moveToHeader(node);
    } else {
        if (map_.size() >= limit_) {
            NodeType* node = static_cast<NodeType*>(UnlinkBefore(header_));
            map_.erase(node->value.first);
            destoryNode(node);
        }
        NodeType* newNode = allocateNode();
        auto myPair = map_.emplace(std::move(key), static_cast<NodeBaseType*>(newNode));
        construct(&newNode->value, myPair.first, std::move(v));
        LinkAfter(header_, newNode);
    }
}

template<typename Key, typename Value, typename Hash, typename Equal>
void LruCache<Key, Value, Hash, Equal>::clear() {
    clearWithoutLink();
    header_->next = header_->prev = header_;
}

} // namespace nemo