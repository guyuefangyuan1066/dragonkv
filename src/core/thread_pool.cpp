#include"thread_pool.hpp"


WorkerThreadPool::WorkerThreadPool(int n,RequestQueue& request_queue,CommandDispatcher & app,HANDLE iocp):
thread_size_(n),request_queue_(request_queue),app_(app),iocp_(iocp){}

void WorkerThreadPool::workerThreadLoop(){
    while(alive_){
        if(request_queue_.empty())continue;
        RequestMessage t;
        if(!request_queue_.dequeue(t)) continue;
        std::cout<<"thread "<<std::this_thread::get_id()<<" processing conne: "<<t.clientFd<<std::endl;
        Connection* conne=t.clientFd;
        conne->postRecv();
        RPCResponse message = app_.dispatch(t.request);
        conne->addRPCResponse(message);
        conne->postSend();
        if (!conne->dequeEmpty()) {
            ULONG_PTR keys = reinterpret_cast<ULONG_PTR>(conne);
            OverlappedEx* ov_buffer_space_avail = new OverlappedEx(OperationType::QUEUE_SPACE_AVAILABLE);
            PostQueuedCompletionStatus(iocp_, 0, keys, reinterpret_cast<LPOVERLAPPED>(ov_buffer_space_avail));
        }
    }
}
void WorkerThreadPool::stop() {
    alive_ = false;
    for (int i = 0; i < thread_size_; ++i) {
        try {
            if (work_[i].joinable()) {
                std::cout << "Joining thread " << i << std::endl;
                work_[i].join();
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in join: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in join." << std::endl;
        }
    }
}
void WorkerThreadPool::start(){
    alive_=true;
    work_.resize(thread_size_);
    for(int i=0;i<thread_size_;++i){
        work_[i]=std::thread(&WorkerThreadPool::workerThreadLoop,this);
    }
}