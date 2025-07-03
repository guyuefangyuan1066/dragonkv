#ifndef CONNECTION_HPP
#define CONNECTION_HPP
#include"../core/MPMCQueue.hpp"
#include"../../include/dragonkv/common.hpp"
#include"protocol/decoder.hpp"
#include"protocol/encoder.hpp"
#include <winsock2.h>
#include <string>
#include <deque>
#include <mutex>
#include <atomic>
#include <mswsock.h>
#include <windows.h>
#include<deque>
//MPMCQueue<RequestItem, 256> requestQueue;

class Connection {
    
    RequestQueue& request_queue_; 
public:

    Connection(int id, SOCKET sock, HANDLE iocp, RequestQueue& request_queue,Decoder& decoder,Encoder& encoder);
    ~Connection();

    void start();                                                      // 启动首次读
    void handleRead(OverlappedEx* ov,size_t n);                        // 回调给业务层
    void handleWrite(OverlappedEx* ov,size_t n);                       // 清除 sendQueue_ 中当前已完成部分
    void handqueuefull(OverlappedEx* ov);
    void close();

    int getId() const;
    SOCKET getSocket() const;
    std::atomic<bool> isAlive() const;
    HANDLE getiocp() const;
    bool dequeEmpty()const;
    void postRecv();                                                       // 投递读
    void postSend(); 
    void addRPCResponse(RPCResponse& mess); 
    
private:
                         // 投递写

    int id_;
    SOCKET socket_;
    HANDLE iocp_;
    std::atomic<bool> alive_;

private:
    std::deque<RequestMessage> dqe_;
    std::mutex mx_dqe1_;
    std::deque<RPCResponse>dqe1_;
    Decoder& decoder_;
    Encoder& encoder_;
};



#endif