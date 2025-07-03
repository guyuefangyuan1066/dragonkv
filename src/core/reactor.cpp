#include "reactor.hpp"
#include <iostream>
#include <mswsock.h>

IOCPReactor::IOCPReactor(int threadCount,RequestQueue& qe,Decoder& decoder) :qe_(qe),decoder_(decoder), iocp_(NULL), listenSocket_(INVALID_SOCKET), running_(false) {
    workers_.resize(threadCount);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

IOCPReactor::~IOCPReactor() {
    stop();
}

bool IOCPReactor::start(unsigned short port) {


    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!iocp_) return false;

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    //inet_addr("192.168.1.100");
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    bind(listenSocket_, (sockaddr*)&addr, sizeof(addr));
    listen(listenSocket_, SOMAXCONN);

    running_ = true;

    accept_=std::move(std::thread([this]() {
        while (running_) {
            SOCKET clientSocket = accept(listenSocket_, nullptr, nullptr);
            if (!running_) { 
                if (clientSocket != INVALID_SOCKET) { 
                    closesocket(clientSocket); 
                }
                break; 
            }
            if (clientSocket != INVALID_SOCKET) {
                addConnection(clientSocket);
            }
        }
    }));

    for (auto& t : workers_) {
        t = std::thread(&IOCPReactor::runLoop, this);
    }

    return true;
}

void IOCPReactor::stop() {
    if (!running_) return;
    running_ = false;

    for (auto& t : workers_) {
        PostQueuedCompletionStatus(iocp_, 0, 0, NULL);
    }

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    if(accept_.joinable()){
        accept_.join();
    }
    closesocket(listenSocket_);
    CloseHandle(iocp_);
    WSACleanup();
}

void IOCPReactor::addConnection(SOCKET clientSocket) {
    int connId = nextConnId_++;
    auto conn = std::make_shared<Connection>(connId, clientSocket, iocp_,qe_,decoder_);
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connections_[connId] = conn;
    }

    CreateIoCompletionPort((HANDLE)clientSocket, iocp_, (ULONG_PTR)conn.get(), 0);
    conn->start();
}

void IOCPReactor::runLoop() {
    while (running_) {
        DWORD bytesTransferred = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL result = GetQueuedCompletionStatus(iocp_, &bytesTransferred, &key, &overlapped, INFINITE);
        
        auto* conn = reinterpret_cast<Connection*>(key);
        auto* ov = reinterpret_cast<OverlappedEx*>(overlapped);
        if (!result || !overlapped){
             int error_code = WSAGetLastError();
            // 常见错误码：ERROR_OPERATION_ABORTED (995), WSA_OPERATION_ABORTED (995) 表示操作被取消
            // WSAECONNRESET, WSAECONNABORTED 等表示连接错误
            std::cerr << "I/O operation failed for socket " << (conn ? conn->getSocket() : -1)
                      << " with error: " << error_code << "\n";

            // 无论如何，关联的 OverlappedEx 都需要释放
            if (ov) {
                delete ov;
            }
            // 如果 conn 仍然有效，确保关闭连接并清理
            if (conn && !conn->isAlive().load(std::memory_order_acquire)) { // 检查是否已标记为不活跃
                // 如果 conn 还没有从 connections_ 中移除，现在移除它
                // 确保 connMutex_ 被锁定
                {
                    std::lock_guard<std::mutex> lock(connMutex_);
                    connections_.erase(conn->getId()); // 假设 Connection 有 getId() 方法
                }
                // 此时，如果 connections_ 中是持有 conn 的最后一个 std::shared_ptr，
                // 那么 conn 就会在这里被自动析构
            }
            delete ov;
            continue; // 继续循环，等待下一个完成包
        }


        if (ov->type == OperationType::READ) {
            conn->handleRead(ov,bytesTransferred);
        } else if (ov->type == OperationType::WRITE) {
            conn->handleWrite(ov,bytesTransferred);
        }else if(ov->type==OperationType::QUEUE_SPACE_AVAILABLE){
            conn->handqueuefull( ov);
        }
        
    }
}
HANDLE IOCPReactor::getiocp() const{
    return iocp_;
}