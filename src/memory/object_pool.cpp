#include "object_pool.hpp"
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include<shared_mutex>
#include "../../include/dragonkv/config.hpp"

// object_pool.cpp
thread_local std::unordered_map<size_t, std::unique_ptr<SlabAllocator>> ObjectPool::localSlabMap_;


void* SlabAllocator:: allocate(){
    void * t= malloc(blockSize_);
    std::lock_guard<std::mutex> lock(slabMutex_);
    if(t==nullptr){
        printf("error in SlabAllocator:: allocate()\n");
        return nullptr;
    }
    ++inUseBlocks_;
    ++totalBlocks_;
    return t;
}
void SlabAllocator::deallocate(void* ptr){
    if(ptr==nullptr&&DEBUG){
        printf("error in SlabAllocator::deallocate\n");
    }
    std::lock_guard<std::mutex> lock(slabMutex_);
    if(freeList_.size()>=10000){
        ::free(ptr); // 释放回系统
        --totalBlocks_;
    }else{
    freeList_.emplace_back(ptr);
    }
    --inUseBlocks_;
}
SlabAllocator::~SlabAllocator(){
    if(totalBlocks_>0){
        // if(inUseBlocks_>0&&DEBUG){
        //     printf("error in SlabAllocator::~SlabAllocator(),inUseBlocks_=%d\n",inUseBlocks_);
        // }
        for(int i=0;i<freeList_.size();++i){
            free(freeList_[i]);
        }
    }
}

ObjectPool& ObjectPool::getInstance(){
    static ObjectPool t;
    return t;
}
size_t ObjectPool::getSuitableSize(size_t size) const {
    for (size_t i = 0; i < FIXED_POOL_COUNT; ++i) {
        if (size <= FIXED_POOL_SIZES[i]) {
            return FIXED_POOL_SIZES[i];
        }
    }
    return 0; // fallback 到 malloc
}

void* ObjectPool::allocate(size_t size) {
    size_t m = getSuitableSize(size);
    void* ptr = nullptr;

    if (m == 0) {
        // 非标准大小，直接 malloc
        ptr = ::malloc(size);
        if (!ptr) return nullptr;
    } else {
        auto& slabMap = localSlabMap_;
        if (slabMap.find(m) == slabMap.end()) {
            slabMap[m] = std::make_unique<SlabAllocator>(m);
        }
        ptr = slabMap[m]->allocate();
    }

    // 注册分配大小（注意：全局表，需加锁）
    {
        std::lock_guard<std::mutex> lock(globalMutex_);
        allocationSizeMap_[ptr] = m == 0 ? size : m;
    }

    return ptr;
}

void ObjectPool::deallocate(void* ptr) {
    if (!ptr) return;

    size_t m;
    {
        std::lock_guard<std::mutex> lock(globalMutex_);
        auto it = allocationSizeMap_.find(ptr);
        if (it == allocationSizeMap_.end()) {
            if (DEBUG) printf("Pointer not from ObjectPool\n");
            return;
        }
        m = it->second;
        allocationSizeMap_.erase(it);
    }

    if (m > FIXED_POOL_SIZES[FIXED_POOL_COUNT - 1]) {
        ::free(ptr); // fallback 内存
        return;
    }

    auto& slabMap = localSlabMap_;
    if (slabMap.find(m) != slabMap.end()) {
        slabMap[m]->deallocate(ptr);
    } else {
        if (DEBUG) printf("Missing slab for deallocation\n");
    }
}




size_t ObjectPool::block_size(void* ptr) const {
    auto it = allocationSizeMap_.find(ptr);
    if (it != allocationSizeMap_.end()) return it->second;
    return 0; // 说明不是本池分配的
}
ObjectPool::ObjectPool() {
    // 可选：打印日志或初始化
}

ObjectPool::~ObjectPool() {
    // 可选：释放资源
}
