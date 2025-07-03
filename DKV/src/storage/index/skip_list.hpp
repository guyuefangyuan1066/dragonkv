#ifndef SKIP_LIST_HPP
#define SKIP_LIST_HPP
#include "../../../include/dragonkv/common.hpp"
#include "../../memory/value_store.hpp"
#include <string>
#include <vector>
#include <functional>
#include <random>
#include <mutex>
#include <shared_mutex>
struct ValueMeta;  

class SkipList {
public:
    explicit SkipList(int maxLevel = 16, float probability = 0.5);
    ~SkipList();
    bool insert(const std::string& key, const ValueMeta& value);
    bool remove(const std::string& key);
    bool get(const std::string& key, ValueMeta& out) const;
    void forEach(const std::function<void(const std::string&, const ValueMeta&)>& func) const;
    void clear(); 
private:
    struct Node {
        std::string key;
        ValueMeta value;
        std::vector<Node*> forward;

        Node(const std::string& k, const ValueMeta& v, int level)
            : key(k), value(v), forward(level, nullptr) {}

        // Node(const char* keyPtr, size_t keyLen, const ValueMeta& value, int level)
        //     : key(keyPtr, keyLen), value(value), forward(level, nullptr) {}
    };

    Node* head_;
    int maxLevel_;
    float probability_;
    int currentLevel_;

    mutable std::shared_mutex mutex_;
    mutable std::default_random_engine randEngine_;
    mutable std::uniform_real_distribution<float> dist_;

    int randomLevel() const;
     
};


#endif