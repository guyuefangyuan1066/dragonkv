#include "value_store.hpp"
#include "object_pool.hpp"  

#include <cstring>



ValueMeta ValueStore::put(const std::string& value) {
    void* mem = pool_.allocate(value.size());
    if(mem!=nullptr){
        std::memcpy(mem, value.data(), value.size());
    }
    return { mem, value.size(), 0 };
}

std::string ValueStore::get(const ValueMeta& meta) {
    return std::string(reinterpret_cast<char*>(meta.dataPtr), meta.length);
}

void ValueStore::remove(const ValueMeta& meta) {
    pool_.deallocate(meta.dataPtr);
}
void* ValueStore::writer_str(const void* data, size_t length) {
    if (length == 0) {
        return nullptr;
    }
    void* mem = pool_.allocate(length);
    return mem;
}