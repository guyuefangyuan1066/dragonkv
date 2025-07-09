// index_manager.hpp
#ifndef INDEX_MANAGER_HPP
#define INDEX_MANAGER_HPP

#include <string>
#include <functional>
#include <shared_mutex>
#include "../../../include/dragonkv/common.hpp"

class SkipList;  

class IndexManager {
    static const int kShardCount = 8;

    struct Shard {
        mutable std::shared_mutex mutex;
        SkipList* skiplist;
    };

    std::array<Shard, kShardCount> shards;

    size_t hashKey(const std::string& key) const {
        return std::hash<std::string>{}(key) % kShardCount;
    }
public:
    IndexManager();
    ~IndexManager();

    void put(const std::string& key, const ValueMeta& value);
    bool get(const std::string& key, ValueMeta& out) const;
    bool remove(const std::string& key);
    void clear();  // 清空所有索引
    void forEach(std::function<void(const std::string&, const ValueMeta&)> func) const;

private:
    //SkipList* skipList_;  
};

#endif
