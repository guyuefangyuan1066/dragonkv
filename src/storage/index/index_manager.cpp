#include "index_manager.hpp"
#include "skip_list.hpp"  

IndexManager::IndexManager() {
    //skipList_ = new SkipList();  
    //shards.fill({std::shared_mutex(), new SkipList()});  // 初始化每个分片的 skiplist
    for (auto& shard : shards) {
        shard.skiplist = new SkipList();
    }
}

IndexManager::~IndexManager() {
    //delete skipList_;
    for (auto& shard : shards) {
        delete shard.skiplist;
    }
}

void IndexManager::put(const std::string& key, const ValueMeta& value) {
    // ValueMeta oldvalue;
    // skipList_->get(key,oldvalue);
    // if(oldvalue.dataPtr!=nullptr){
    //     //.....
    //     ValueStore m(ObjectPool::getInstance()).remove(oldvalue.ptr); 
    // }
    //skipList_->insert(key, value);
    size_t index = hashKey(key);
    auto& shard = shards[index];
    std::unique_lock lock(shard.mutex);
    shard.skiplist->insert(key, value);
}

bool IndexManager::get(const std::string& key, ValueMeta& out) const {
    //return skipList_->get(key, out);
    //hashKey函数似乎不允许使用const值，我应该怎样处理？
    size_t index = hashKey(key);
    auto& shard = shards[index];
    std::shared_lock lock(shard.mutex);
    return shard.skiplist->get(key, out);
}

bool IndexManager::remove(const std::string& key) {
    //return skipList_->remove(key);
    size_t index = hashKey(key);
    auto& shard = shards[index];
    std::unique_lock lock(shard.mutex);
    return shard.skiplist->remove(key);
}

void IndexManager::clear() {
    for (auto& shard : shards) {
        std::unique_lock lock(shard.mutex);
        shard.skiplist->clear();
    }
}

void IndexManager::forEach(std::function<void(const std::string&, const ValueMeta&)> func) const {
    //skipList_->forEach(func);
    for (const auto& shard : shards) {
        std::shared_lock lock(shard.mutex);
        shard.skiplist->forEach(func);
    }
}
