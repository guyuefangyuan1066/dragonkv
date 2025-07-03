#ifndef REACTOR_HPP
#define REACTOR_HPP

#include <mswsock.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include<vector>
#include<atomic>
#include"../network/connection.hpp"
#include <winsock2.h>
#include <string>
#include <mutex>
#include"../../include/dragonkv/common.hpp"

class IOCPReactor {
public:
    IOCPReactor(int threadCount ,RequestQueue& qe,Decoder& decoder_);
    ~IOCPReactor();

    bool start(unsigned short port);                // 启动监听 + 线程
    void stop();                                    // 停止运行
    void runLoop();                                 // IOCP主循环
    void addConnection(SOCKET clientSocket);        // 接受连接
    HANDLE getiocp() const;
private:
    HANDLE iocp_;
    SOCKET listenSocket_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_;
    std::thread accept_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    std::mutex connMutex_;
    std::atomic<int> nextConnId_{1};
    RequestQueue& qe_; 
    Decoder& decoder_;
};

#endif