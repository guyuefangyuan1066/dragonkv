#ifndef DRAGONKV_STORAGE_ENGINE_HPP
#define DRAGONKV_STORAGE_ENGINE_HPP

#include <string>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex> 
#include "index/index_manager.hpp"
#include "wal/wal_writer.hpp"
#include "../memory/object_pool.hpp"
#include "../memory/value_store.hpp"
#include "../../include/dragonkv/common.hpp" 

class StorageEngine {
public:
    explicit StorageEngine(const std::string& walPath);
    ~StorageEngine() ;
    // 写入键值对（覆盖旧值）
    bool put(const std::string& key, const std::string& value);
    // 查询键值对（返回值为可选，未找到返回 nullopt）
    std::optional<std::string> get(const std::string& key);
    bool remove(const std::string& key);
    // 加载恢复 WAL 日志（在构造函数或手动触发）
    void recover(uint64_t maxTimestamp = UINT64_MAX);
private:
    std::unique_ptr<WALWriter> wal_;
    std::unique_ptr<IndexManager> index_;
    std::unique_ptr<ValueStore> valStore_;
    //mutable std::mutex engineMutex_; // 防止并发 put/remove 时竞态
    struct Record {
    uint64_t ts;
    std::string value;
    };
    mutable std::shared_mutex engineRWLock_;
};

#endif 
