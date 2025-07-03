#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP
#include<vector>
#include<thread>
#include<atomic>
#include"../../include/dragonkv/common.hpp"
#include"../network/connection.hpp"
#include"../network/protocol/encoder.hpp"
#include"MPMCQueue.hpp"
#include"../app/command_dispatcher.hpp"
class CommandDispatcher;

class WorkerThreadPool{
    int thread_size_;
    std::atomic<bool> alive_;
    std::vector<std::thread> work_;
    HANDLE iocp_;
    private:

    CommandDispatcher & app_; 
    RequestQueue& request_queue_; 
    //ResponseQueue& response_queue_;

    public:
    WorkerThreadPool(int n,RequestQueue& request_queue,CommandDispatcher & app,const HANDLE iocp);
    void stop();
    void start();
    void workerThreadLoop();
};

#endif