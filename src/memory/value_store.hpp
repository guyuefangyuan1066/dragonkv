#ifndef VALUE_STORE_HPP
#define VALUE_STORE_HPP
#include<stdio.h>
#include <cstdint>
#include<string>
#include "../../include/dragonkv/common.hpp"
class ObjectPool;
class ValueStore {
public:
    ValueStore(ObjectPool& pool) : pool_(pool){}
    ValueMeta put(const std::string& value);
    std::string get(const ValueMeta& meta);
    void* writer_str(const void* data, size_t length);
    void remove(const ValueMeta& meta);

private:
    ObjectPool& pool_ ;
    
};

#endif