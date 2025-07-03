#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>
#include <unordered_map>
#include <map>
#include <mutex>
#include <memory>
#include <cstddef>
#include <algorithm>
#include <iostream>
#include<shared_mutex>
class SlabAllocator {
public:
    explicit SlabAllocator(size_t blockSize)
        : blockSize_(blockSize), totalBlocks_(0), inUseBlocks_(0) {}

    ~SlabAllocator();

    void* allocate();
    void deallocate(void* ptr);
    size_t blockSize() const { return blockSize_; }
    size_t freeCount() const { return freeList_.size(); }
    
private:
    size_t blockSize_;
    size_t totalBlocks_;
    size_t inUseBlocks_;
    std::vector<void*> freeList_;
    std::mutex slabMutex_;
};

class ObjectPool {
public:
    // 获取单例实例
    static ObjectPool& getInstance();

    // 禁用拷贝/移动
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // 主接口：分配与回收
    void* allocate(size_t size);
    void deallocate(void* ptr);
    void clear();
    size_t block_size(void* ptr) const;
    // 诊断接口
    void printStats() const;

private:
    ObjectPool();
    ~ObjectPool();
    size_t getSuitableSize(size_t size) const;

private:
    static constexpr size_t FIXED_POOL_SIZES[] = {64, 128, 256, 512, 1024, 2048, 4096};
    static constexpr size_t FIXED_POOL_COUNT = sizeof(FIXED_POOL_SIZES) / sizeof(FIXED_POOL_SIZES[0]);

    std::map<size_t, std::unique_ptr<SlabAllocator>> slabMap_;
    mutable std::mutex globalMutex_;
    std::shared_mutex poolMutex_;
    // 辅助记录分配块大小（也可使用 chunk header 技术）
    std::map<void*, size_t> allocationSizeMap_;
    static thread_local std::unordered_map<size_t, std::unique_ptr<SlabAllocator>> localSlabMap_;
};

#endif // OBJECT_POOL_HPP

