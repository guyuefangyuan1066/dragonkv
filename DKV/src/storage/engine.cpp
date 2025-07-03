#include"engine.hpp"
#include <memory>
#include"../../include/dragonkv/config.hpp"
StorageEngine::StorageEngine(const std::string& walPath){
    wal_=std::make_unique<WALWriter>(walPath);
    index_=std::make_unique<IndexManager>();
    valStore_ = std::make_unique<ValueStore>(ObjectPool::getInstance());
}

StorageEngine::~StorageEngine() {
    //std::cout<<"~StorageEngine()\n";
    // 1. 停止WAL写入线程
    if (wal_) {
        wal_->close(); // 添加停止方法
    }
    //std::cout<<"wal_->close()\n";
    // 2. 释放资源
    valStore_.reset();
    index_.reset();
    wal_.reset();
    //std::cout<<".reset()\n";
    // 3. 等待所有后台操作完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //td::cout<<"sleep_for\n";
}

bool StorageEngine::put(const std::string& key, const std::string& value){
    std::unique_lock<std::shared_mutex> lock(engineRWLock_);
    //std::shared_lock<std::shared_mutex> lock(engineRWLock_);
    ValueMeta newMeta = valStore_->put(value);
    if(newMeta.dataPtr != nullptr){
        ValueMeta oldMeta;
        if(index_->get(key, oldMeta)){
            valStore_->remove(oldMeta);  
        }
        index_->put(key, newMeta);
        if(!wal_->append(key, value) && DEBUG){
            std::cout<<"wal_append error!\n";
        }
        return true;
    }
    valStore_->remove(newMeta);
    return false;
}

std::optional<std::string> StorageEngine::get(const std::string& key){
    ValueMeta out;
    if (index_->get(key, out)) {
        return valStore_->get(out);
    }
    return std::nullopt;
}

bool StorageEngine::remove(const std::string& key){
    std::unique_lock<std::shared_mutex> lock(engineRWLock_);
    //std::shared_lock<std::shared_mutex> lock(engineRWLock_);
    ValueMeta out;
    if(index_->get(key, out)){
        if(index_->remove(key)){
            valStore_->remove(out);
            wal_->append(key, ""); 
            return true;
        }
    }
    return false;
}

void StorageEngine::recover(uint64_t maxTimestamp ){
    std::unordered_map<std::string, Record>mp;
    auto recordCollector = [&](const std::string& key, const std::string& value, uint64_t& ts) {
        auto it = mp.find(key);
        if (it == mp.end()) {
            mp[key] = { ts, value };
        } else {
            const auto& existing = it->second;
            if (ts > existing.ts) {
                mp[key] = { ts, value };
            }
        }
    };
    wal_->recoverUpTo(maxTimestamp,recordCollector);
    std::unique_lock<std::shared_mutex> lock(engineRWLock_);
    for(auto& p:mp){
        if (p.second.value.empty()) {
            this->remove(p.first);
        } else {
            this->put(p.first, p.second.value);
        }
    }
}