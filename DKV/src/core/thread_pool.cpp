#include"thread_pool.hpp"


WorkerThreadPool::WorkerThreadPool(int n,RequestQueue& request_queue,CommandDispatcher & app,HANDLE iocp):
thread_size_(n),request_queue_(request_queue),app_(app),iocp_(iocp){}

void WorkerThreadPool::workerThreadLoop(){
    while(alive_){
        if(request_queue_.empty())continue;
        RequestMessage t;
        request_queue_.dequeue(t);//从消息队列取出 查询消息
        Connection* conne=t.clientFd;
        RPCResponse message = app_.dispatch(t.request);//调用应用层的接口，从数据引擎获得封装后的消息
        //现在需要将得到的message发送给网络线程，由网络线程负责解包成string，问题在于，如何将其提交给网络线程。
        //原方案，将实际的写操作和消息拷贝进ov放在工作线程，并通知iocp，此时只需处理一种问题，就是查询消息队列的满，为满这种特殊消息单独编写一个回调
        //方案1：直接交给conne专门负责缓存消息的属性，但需要加锁，因为此时可能在postsend方法中，取出缓存的结果，防止将std::deque数据写坏
        //方案2：再一次创建一个无锁队列，但是由会发生早先遇到的问题，就是查询太多处理不过来， 队列满，
        //其次关于iocp_的问题，在connection类只是存在一个副本，不进行析构，暂时先不管
        //或者由你给出一个可行的方案
        conne->addRPCResponse(message);
        conne->postSend();
        if (!conne->dequeEmpty()) {
            ULONG_PTR keys = reinterpret_cast<ULONG_PTR>(conne);
            OverlappedEx* ov_buffer_space_avail = new OverlappedEx(OperationType::QUEUE_SPACE_AVAILABLE);
            PostQueuedCompletionStatus(iocp_, 0, keys, reinterpret_cast<LPOVERLAPPED>(ov_buffer_space_avail));
        }
    }
}
void WorkerThreadPool::stop(){
    alive_=false;
    for(int i=0;i<thread_size_;++i){
        if(work_[i].joinable()){
            work_[i].join();
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