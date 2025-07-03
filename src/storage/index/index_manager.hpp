// index_manager.hpp
#ifndef INDEX_MANAGER_HPP
#define INDEX_MANAGER_HPP

#include <string>
#include <functional>
#include "../../../include/dragonkv/common.hpp"

class SkipList;  

class IndexManager {
public:
    IndexManager();
    ~IndexManager();

    void put(const std::string& key, const ValueMeta& value);
    bool get(const std::string& key, ValueMeta& out) const;
    bool remove(const std::string& key);
    void clear();  // 清空所有索引
    void forEach(std::function<void(const std::string&, const ValueMeta&)> func) const;

private:
    SkipList* skipList_;  
};

#endif
