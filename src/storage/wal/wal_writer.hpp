#ifndef WAL_WRITER_HPP
#define WAL_WRITER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>

class WALWriter {
public:
    explicit WALWriter(const std::string& logDir, size_t maxFileSize = 4 * 1024 * 1024,std::chrono::milliseconds flushInterval = std::chrono::milliseconds(100)); // 可配置文件滚动阈值
    ~WALWriter();

    bool append(const std::string& key, const std::string& value);
    void flush();
    void sync();
    void close();

    // 恢复直到指定时间戳（含）
    bool recoverUpTo(uint64_t maxTimestamp,
        const std::function<void(const std::string&, const std::string&, uint64_t& ts)>& callback);

private:
    void flusherLoop();              // 后台写线程
    void rollIfNeeded(size_t nextRecordSize);            // 文件滚动
    void openNewFile();
    std::string generateLogFileName(); // 生成带时间戳的文件名

private:
    std::ofstream logFile_;
    std::string logDir_="../log";
    std::string logFilePath_;
    std::mutex writeMutex_;
    FILE* file = nullptr;
    std::queue<std::string> logQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCond_;

    std::atomic<bool> stop_{false};
    std::thread flusherThread_;
    std::chrono::milliseconds flushInterval_;
    size_t maxFileSize_=4*1024*1024;
    size_t currentFileSize_ = 0;
};

#endif
