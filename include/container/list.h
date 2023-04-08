#pragma once

namespace nemo {

struct ListNodeBase {
    ListNodeBase* next = nullptr;
};

inline ListNodeBase* Link(ListNodeBase* target, ListNodeBase* node) {
    target->next = node;
    return target;
}

inline ListNodeBase* Unlink(ListNodeBase* target) {
    ListNodeBase* tmp = target->next;
    if (tmp->next) {
        target->next = tmp->next;
    }

    return tmp;
}

struct DoubleLinkedNodeBase {
    DoubleLinkedNodeBase* prev;
    DoubleLinkedNodeBase* next;
};

template<typename T>
struct DoubleLinkedNode : public DoubleLinkedNodeBase {
    T value;
};

inline DoubleLinkedNodeBase* UnlinkSelfUnckecked(DoubleLinkedNodeBase* node) {
    DoubleLinkedNodeBase* prevNode = node->prev;
    DoubleLinkedNodeBase* nextNode = node->next;
    prevNode->next = nextNode;
    nextNode->prev = prevNode;
    return node;
}

inline DoubleLinkedNodeBase* UnlinkSelf(DoubleLinkedNodeBase* node) {
    if (nullptr == node || (nullptr == node->prev && nullptr == node->next) || 
            (node == node->prev && node == node->next)) {
        return nullptr;
    }
    if (nullptr == node->prev) {
        node->next->prev = nullptr;
    } else if (nullptr == node->next) {
        node->prev->next = nullptr;
    } else {
        UnlinkSelfUnckecked(node);
    }

    return node;
}

[[nodiscard]] inline DoubleLinkedNodeBase* UnlinkAfter(DoubleLinkedNodeBase* node) {
    if (nullptr == node || nullptr == node->next || node == node->next) {
        return nullptr;
    }

    DoubleLinkedNodeBase* nextNode = node->next;
    nextNode->next->prev = node;
    node->next = nextNode->next;
    return nextNode;
}

[[nodiscard]] inline DoubleLinkedNodeBase* UnlinkBefore(DoubleLinkedNodeBase* node) {
    if (nullptr == node || nullptr == node->prev || node == node->prev) {
        return nullptr;
    }

    DoubleLinkedNodeBase* prevNode = node->prev;
    prevNode->prev->next = node;
    node->prev = prevNode->prev;
    return prevNode;
}

inline void LinkAfter(DoubleLinkedNodeBase* linkedNode, DoubleLinkedNodeBase* linkingNode) {
    linkingNode->next = linkedNode->next;
    linkingNode->prev = linkedNode;
    linkedNode->next->prev = linkingNode;
    linkedNode->next = linkingNode;
}

inline void LinkBefore(DoubleLinkedNodeBase* linkedNode, DoubleLinkedNodeBase* linkingNode) {
    linkingNode->prev = linkedNode->prev;
    linkingNode->next = linkedNode;
    linkedNode->prev->next = linkingNode;
    linkedNode->prev = linkingNode;
}

} // namespace nemo