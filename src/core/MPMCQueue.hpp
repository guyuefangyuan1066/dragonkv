#ifndef MPMCQUEUE_HPP
#define MPMCQUEUE_HPP

#include <array>
#include <atomic>
#include <cstddef> // For size_t
#include <thread>  // For std::this_thread::yield()
#include <type_traits> // For std::aligned_storage_t

// 定义缓存行大小，通常为64字节
constexpr size_t CACHE_LINE_SIZE = 64;

// 对齐到缓存行边界
#define CACHELINE_ALIGNED alignas(CACHE_LINE_SIZE)

template<typename T>
struct alignas(CACHE_LINE_SIZE) Cell {
    // 序列号：用于同步生产者和消费者，判断槽位状态，并解决ABA问题
    std::atomic<size_t> sequence;
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
    MPMCQueue() = default;

    // 非拷贝构造和赋值，因为原子变量不能拷贝
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    // 析构函数：确保销毁缓冲区中所有已构造的对象
    ~MPMCQueue() {
        // 只有当队列被安全关闭且没有并发操作时才调用此析构函数
        // 在高并发场景下，安全析构一个无锁队列本身就是个复杂问题
        // 对于毕业设计，可以假设在所有生产者消费者都停止后才析构
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
        // 生产者获取一个写入“票据”或“序列号”
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed); // (1) 生产者预读取当前位置
        size_t cell_index;
        Cell<T>* cell;

        while (true) {
            cell_index = pos & capacity_mask_; // 获取物理索引
            cell = &buffer_[cell_index];
            size_t cell_sequence = cell->sequence.load(std::memory_order_acquire); // (2) 生产者读取槽位的序列号

            long diff = (long)cell_sequence - (long)pos; // 判断槽位状态

            if (diff == 0) { // 槽位序列号与期望的写入序列号匹配，表示该槽位是空的，可写入
                // (3) 尝试原子地将自己的写入位置从 pos 更新为 pos + 1
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break; // 成功预订槽位
                }
                // 如果CAS失败，说明其他生产者已经抢先，pos被更新，需要重新循环
            } else if (diff < 0) { // cell_sequence < pos, 说明队列已满，或乱序
                // 队列已满，或遇到了一个还未被消费者清空的槽位
                // 此时，pos 应该就是 dequeue_pos_ 的值 + Capacity，表明队列已满
                if (pos - dequeue_pos_.load(std::memory_order_relaxed) >= Capacity) {
                    return false; // 队列已满
                }
                // 如果不是真正满，可能存在竞争，重新尝试
                pos = enqueue_pos_.load(std::memory_order_relaxed); // 重新读取最新pos
                std::this_thread::yield(); // 退让，避免忙等待
            } else { // diff > 0, cell_sequence > pos, 应该不会发生这种预订，除非是多生产者下的乱序
                // 这通常表示消费者清空槽位过快，或者发生了某种异常情况。
                // 重新尝试通常是安全的
                pos = enqueue_pos_.load(std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }

        // 此时，pos 是当前生产者成功预订的写入位置
        // (4) 使用定位new构造对象，避免默认构造函数
        new (cell->data_ptr()) T(item);

        // (5) 更新槽位序列号，让消费者可见
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    // 出队操作 (多消费者)
    bool dequeue(T& item) {
        // 消费者获取一个读取“票据”或“序列号”
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed); // (1) 消费者预读取当前位置
        size_t cell_index;
        Cell<T>* cell;

        while (true) {
            cell_index = pos & capacity_mask_; // 获取物理索引
            cell = &buffer_[cell_index];
            size_t cell_sequence = cell->sequence.load(std::memory_order_acquire); // (2) 消费者读取槽位的序列号

            long diff = (long)cell_sequence - (long)(pos + 1); // 判断槽位状态 (期望值是 pos + 1)

            if (diff == 0) { // 槽位序列号与期望的读取序列号匹配，表示该槽位是已写入的，可读取
                // (3) 尝试原子地将自己的读取位置从 pos 更新为 pos + 1
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break; // 成功预订槽位
                }
                // 如果CAS失败，说明其他消费者已经抢先，pos被更新，需要重新循环
            } else if (diff < 0) { // cell_sequence < pos + 1, 队列已空，或乱序
                // 队列已空，或遇到了一个还未被生产者写入的槽位
                // 此时，pos 应该就是 enqueue_pos_ 的值，表明队列已空
                if (pos >= enqueue_pos_.load(std::memory_order_relaxed)) {
                    return false; // 队列已空
                }
                // 如果不是真正空，可能存在竞争，重新尝试
                pos = dequeue_pos_.load(std::memory_order_relaxed); // 重新读取最新pos
                std::this_thread::yield(); // 退让
            } else { // diff > 0, cell_sequence > pos + 1, 应该不会发生，除非是多消费者下的乱序
                // 消费者清空槽位过快
                pos = dequeue_pos_.load(std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }

        // 此时，pos 是当前消费者成功预订的读取位置
        // (4) 从数据区移动或拷贝数据
        item = std::move(*(cell->data_ptr())); // 优先使用移动语义
        cell->data_ptr()->~T(); // 手动调用T的析构函数

        // (5) 更新槽位序列号，让生产者可见（下次写此槽位时期望的序列号是 pos + Capacity）
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

// --- 使用示例 (概念性) ---
// // Request 队列
// MPMCQueue<RequestItem, 256> requestQueue;
// // Response 队列
// MPMCQueue<ResponsePackage, 256> responseQueue;
#endif