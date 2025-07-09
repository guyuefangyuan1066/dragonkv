#ifndef MPMCQUEUE_HPP
#define MPMCQUEUE_HPP

#include <array>
#include <atomic>
#include <cstddef> 
#include <thread>  
#include <type_traits> 
#include<iostream>
constexpr size_t CACHE_LINE_SIZE = 64;

#define CACHELINE_ALIGNED alignas(CACHE_LINE_SIZE)

template<typename T>
struct alignas(CACHE_LINE_SIZE) Cell {
    // 序列号：用于同步生产者和消费者，判断槽位状态，并解决ABA问题
    std::atomic<size_t> sequence{0};
    // 实际数据：使用 union + aligned_storage_t 避免T的默认构造和析构，
    // 实现手动管理对象的生命周期，支持non-POD类型。
    // alignas(alignof(T)) char data_storage[sizeof(T)]; // 另一种简单写法，但需要手动对齐
    std::aligned_storage_t<sizeof(T), alignof(T)> data_storage;

    // 辅助方法，用于访问 data_storage 中的T对象
    T* data_ptr() { return reinterpret_cast<T*>(&data_storage); }
    const T* data_ptr() const { return reinterpret_cast<const T*>(&data_storage); }
};

template<typename T, size_t Capacity>
class MPMCQueue {
    // 队列实际容量为 Capacity，而不是 Capacity - 1。
    // 这要求 Capacity 是 2 的幂次方，方便位运算取模。
    static_assert((Capacity > 0) && ((Capacity & (Capacity - 1)) == 0), "Capacity must be a power of two and greater than zero.");

public:
    MPMCQueue() {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // 非拷贝构造和赋值，因为原子变量不能拷贝
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;
    ~MPMCQueue() {
        size_t head = dequeue_pos_.load(std::memory_order_relaxed);
        size_t tail = enqueue_pos_.load(std::memory_order_relaxed);

        while (head < tail) { // 遍历所有可能已构造的对象
            size_t index = head & capacity_mask_; // 等同于 head % Capacity
            Cell<T>& cell = buffer_[index];

            // 检查序列号以确保对象是有效构造的
            // 如果cell.sequence.load() == head + 1，表示该对象已被生产者写入并准备好被消费
            if (cell.sequence.load(std::memory_order_acquire) == head + 1) {
                cell.data_ptr()->~T(); // 调用T的析构函数
            }
            head++;
        }
    }

    // 入队操作 (多生产者)
    bool enqueue(const T& item) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed); 
        //std::cout<<"enqueue pos:"<<pos<<"\n";
        size_t cell_index;
        Cell<T>* cell;
        while (true) {
            cell_index = pos & capacity_mask_; 
            cell = &buffer_[cell_index];
            size_t cell_sequence = cell->sequence.load(std::memory_order_acquire); 

            long diff = (long)cell_sequence - (long)pos; 
            //std::cout<<diff<<"\n";
            if (diff == 0) { 
                
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break; 
                }
            } else if (diff < 0) { 
                if (pos - dequeue_pos_.load(std::memory_order_relaxed) >= Capacity) {
                    return false; 
                }
                pos = enqueue_pos_.load(std::memory_order_relaxed);
                std::this_thread::yield(); 
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
        new (cell->data_ptr()) T(item);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

        // 出队操作 (多消费者)
    bool dequeue(T& item) {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed); 
        size_t cell_index;
        Cell<T>* cell;

        while (true) {
            cell_index = pos & capacity_mask_; 
            cell = &buffer_[cell_index];
            size_t cell_sequence = cell->sequence.load(std::memory_order_acquire);
            long diff = (long)cell_sequence - (long)(pos + 1);
            if (diff == 0) { 
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                    break; 
                }
            } else if (diff < 0) {
                if (pos >= enqueue_pos_.load(std::memory_order_acquire)) {
                    return false;
                }
                pos = dequeue_pos_.load(std::memory_order_relaxed);
                std::this_thread::yield(); 
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
        item = std::move(*(cell->data_ptr()));
        cell->data_ptr()->~T();
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return true;
    }

    // 辅助方法，用于判断队列是否为空（近似）
    bool empty() const {
        return dequeue_pos_.load(std::memory_order_relaxed) >= enqueue_pos_.load(std::memory_order_relaxed);
    }

    // 辅助方法，用于判断队列是否已满（近似）
    bool full() const {
        return enqueue_pos_.load(std::memory_order_relaxed) - dequeue_pos_.load(std::memory_order_relaxed) >= Capacity;
    }

private:
    CACHELINE_ALIGNED std::array<Cell<T>, Capacity> buffer_;
    CACHELINE_ALIGNED std::atomic<size_t> enqueue_pos_{0};  // 下一个生产者尝试写入的逻辑序列号
    CACHELINE_ALIGNED std::atomic<size_t> dequeue_pos_{0};  // 下一个消费者尝试读取的逻辑序列号
    const size_t capacity_mask_ = Capacity - 1; // 用于高效取模运算
};
#endif