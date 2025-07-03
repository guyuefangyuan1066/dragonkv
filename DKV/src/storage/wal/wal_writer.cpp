#include "wal_writer.hpp"
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <io.h>
#include <windows.h>

// 内部 encode
namespace {
    std::string encode(const std::string& key, const std::string& value, uint64_t ts) {
        std::string result;
        uint32_t klen = key.size(), vlen = value.size();
        result.append(reinterpret_cast<const char*>(&ts), sizeof(uint64_t));
        result.append(reinterpret_cast<const char*>(&klen), sizeof(uint32_t));
        result.append(reinterpret_cast<const char*>(&vlen), sizeof(uint32_t));
        result.append(key).append(value);
        return result;
    }
}

WALWriter::WALWriter(const std::string& logDir, size_t maxFileSize,std::chrono::milliseconds flushInterval)
    : logDir_(logDir), maxFileSize_(maxFileSize) ,flushInterval_(flushInterval){

    if (!std::filesystem::exists(logDir_))
        std::filesystem::create_directories(logDir_);

    logFilePath_ = generateLogFileName();
    logFile_.open(logFilePath_, std::ios::binary | std::ios::app);
    if (!logFile_.is_open())
        throw std::runtime_error("Failed to open WAL file: " + logFilePath_);

    flusherThread_ = std::thread(&WALWriter::flusherLoop, this);
}

WALWriter::~WALWriter() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        //std::cout<<"queueMutex_ \n";
        stop_ = true;
    }
    queueCond_.notify_one();
    //std::cout<<"notify_one() \n";
    if (flusherThread_.joinable())
        flusherThread_.join();
    flush();
    close();
}
bool WALWriter::append(const std::string& key, const std::string& value) {
    uint64_t ts = std::chrono::system_clock::now().time_since_epoch().count();
    std::string record = encode(key, value, ts);
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (stop_) return false;
        logQueue_.push(std::move(record));
    }
    queueCond_.notify_one();
    return true;
}






void WALWriter::rollIfNeeded(size_t nextSize) {
    // 用 WinAPI 获取真实文件大小
    
    errno_t err = fopen_s(&file, logFilePath_.c_str(), "rb");
    if (err == 0 && file) {
        _fseeki64(file, 0, SEEK_END);
        currentFileSize_ = _ftelli64(file);
        fclose(file);
    }

    if (currentFileSize_ + nextSize >= maxFileSize_) {
        logFile_.flush();
        logFile_.close();
        logFilePath_ = generateLogFileName();
        logFile_.open(logFilePath_, std::ios::binary | std::ios::app);
        currentFileSize_ = 0;
    }
}


void WALWriter::flusherLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        auto deadline = std::chrono::steady_clock::now() + flushInterval_;
        queueCond_.wait_until(lock, deadline,[this]() { return !logQueue_.empty() || stop_; });
        std::queue<std::string> localQueue;
        std::swap(localQueue, logQueue_);
        bool shouldExit = stop_;
        lock.unlock();

        if (!localQueue.empty()) {
            while (!localQueue.empty()) {
                const std::string& rec = localQueue.front();
                rollIfNeeded(rec.size());
                logFile_.write(rec.data(), rec.size());
                currentFileSize_ += rec.size();
                localQueue.pop();
            }
            logFile_.flush();
        }
        flush();
        if (shouldExit && localQueue.empty()) {
            break;
        }
    }
}



void WALWriter::flush() {
    logFile_.flush();
    if (file != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(file);
    }
}

void WALWriter::sync() {
    std::lock_guard<std::mutex> lock(writeMutex_);
    logFile_.flush();

    FILE* file = nullptr;
    errno_t err = fopen_s(&file, logFilePath_.c_str(), "ab");
    if (err == 0 && file) {
        int fd = _fileno(file);
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(hFile);
        }
        fclose(file);
    }
}

void WALWriter::close() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    queueCond_.notify_one();
    if (flusherThread_.joinable())
        flusherThread_.join();
    flush();
    //close();
}
bool WALWriter::recoverUpTo(uint64_t maxTs,
    const std::function<void(const std::string&, const std::string&, uint64_t& ts)>& callback) {

    for (const auto& entry : std::filesystem::directory_iterator(logDir_)) {
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in.is_open()) continue;

        while (in.peek() != EOF) {
            uint64_t ts;
            uint32_t klen, vlen;
            in.read(reinterpret_cast<char*>(&ts), sizeof(uint64_t));
            in.read(reinterpret_cast<char*>(&klen), sizeof(uint32_t));
            in.read(reinterpret_cast<char*>(&vlen), sizeof(uint32_t));
            if (ts > maxTs || klen > 64 * 1024 || vlen > 64 * 1024) break;
            std::string key(klen, '\0'), val(vlen, '\0');
            in.read(&key[0], klen);
            in.read(&val[0], vlen);
            if (!in) break;

            callback(key, val,ts);
        }
    }
    return true;
}

std::string WALWriter::generateLogFileName() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmStruct{};
#ifdef _WIN32
    localtime_s(&tmStruct, &t);
#else
    localtime_r(&t, &tmStruct);
#endif
    std::ostringstream oss;
    oss << logDir_ << "/wal_"
        << std::put_time(&tmStruct, "%Y%m%d_%H%M%S") << ".log";
    return oss.str();
}

