#include "reactor.hpp"
#include <iostream>
#include <mswsock.h>

IOCPReactor::IOCPReactor(int threadCount,RequestQueue& qe,Decoder& decoder_,Encoder& encoder_,ValueStore& valStore) :qe_(qe),decoder_(decoder_),encoder_(encoder_),valStore_(valStore), iocp_(NULL), listenSocket_(INVALID_SOCKET), running_(false) {
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
    std::cout << "IOCPReactor stop begin..." << std::endl;
    if (!running_) return;
    running_ .store(false, std::memory_order_release);

    for (auto& t : workers_) {
        PostQueuedCompletionStatus(iocp_, 0, 0, NULL);
    }

    for (auto& t : workers_) {
        try {
            if (t.joinable()) {
                std::cout << "Joining thread " << std::this_thread::get_id() << std::endl;
                t.join();
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in join: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in join." << std::endl;
        }
    }
    closesocket(listenSocket_);
    if(accept_.joinable()){
        accept_.join();
    }
    
    CloseHandle(iocp_);
    WSACleanup();
}

void IOCPReactor::addConnection(SOCKET clientSocket) {
    int connId = nextConnId_++;
    auto conn = std::make_shared<Connection>(connId, clientSocket, iocp_,qe_,decoder_,encoder_, valStore_);
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
        if(overlapped==nullptr){
            // 退出信号，直接 break，不再访问 key/conn/ov
            break;
        }
        auto* conn = reinterpret_cast<Connection*>(key);
        auto* ov = reinterpret_cast<OverlappedEx*>(overlapped);
        if (!result){
            int error_code = WSAGetLastError();
            std::cerr << "I/O operation failed for socket " << (conn ? conn->getSocket() : -1)
                      << " with error: " << error_code << "\n";
            if (ov) {
                delete ov;
            }
            if (conn && !conn->isAlive().load(std::memory_order_acquire)) { 
                std::lock_guard<std::mutex> lock(connMutex_);
                connections_.erase(conn->getId()); 
            }
            continue;
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