#include"connection.hpp"
//读回调函数，处理接收到的信息
void Connection::handleRead(OverlappedEx* ov,size_t n){
    decoder_.appendData(ov->buffer,n);
    auto ret =decoder_.parseAllAvailableRequests();
    for(int i=0;i<ret.size();++i){
        if(!request_queue_.full()){
            //RequestMessage  cnt={this,ret[i]};
            if(request_queue_.enqueue({this,ret[i]})){
                //std::cout<<"Enqueued request "<<ret[i].key<<" "<<ret[i].type<<" in thread "<<std::this_thread::get_id()<<std::endl;
            }else{
                //std::cout<<"Failed to enqueue request "<<ret[i].key<<" "<<ret[i].type<<" in thread "<<std::this_thread::get_id()<<std::endl;
            }
        }else{
            dqe_.push_back({this,ret[i]});
        }
    }
    ov->~OverlappedEx(); // 手动析构
    valStore_.remove({ov, sizeof(OverlappedEx), 0}); // 归还内存池
}
void Connection::handleWrite(OverlappedEx* ov, size_t n) {
    if (n == 0) {
        close();
    } else {
        // 检查是否还有待发送的数据
        std::lock_guard<std::mutex> lock(mx_dqe1_);
        if (!dqe1_.empty()) {
            postSend(); // 继续发送剩余数据
        }
    }
    //delete ov;
    ov->~OverlappedEx(); // 手动析构
    valStore_.remove({ov, sizeof(OverlappedEx), 0}); // 归还内存池
}
void Connection::handqueuefull(OverlappedEx* ov) {
    while (!dqe_.empty() && !request_queue_.full()) {
        request_queue_.enqueue(dqe_.front());
        dqe_.pop_front();
    }
    // 如果还有剩余，继续通知 IOCP
    if (!dqe_.empty()) {
        ULONG_PTR keys = reinterpret_cast<ULONG_PTR>(this);
        OverlappedEx* ov_buffer = new OverlappedEx(OperationType::QUEUE_SPACE_AVAILABLE);
        PostQueuedCompletionStatus(iocp_, 0, keys, reinterpret_cast<LPOVERLAPPED>(ov_buffer));
    }
    delete ov;
}

void Connection::postRecv(){
    //OverlappedEx* ov_ex = new OverlappedEx(OperationType::READ);
    void* mem = valStore_.writer_str(nullptr, sizeof(OverlappedEx));
    if(mem==nullptr){
        // 分配失败，处理错误
        close();
        return;
    }
    OverlappedEx* ov_ex = new (mem) OverlappedEx(OperationType::READ);
    if (ov_ex == nullptr) {
        std::cerr << "Failed to allocate memory for OverlappedEx\n";
        close();
        return;
    }
    if (!ov_ex) {
        close();
        return;
    }

    WSABUF buf;
    buf.buf = ov_ex->buffer;
    buf.len = sizeof(ov_ex->buffer);
    DWORD flags = 0;
    int ret = WSARecv(socket_, &buf, 1, nullptr, &flags, &ov_ex->overlapped, nullptr);
    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {

            std::cerr << "WSARecv failed for socket " << socket_ << " with error: " << err << "\n";
            delete ov_ex;
            close(); 
        }         
    }     
}

 
void Connection::postSend() {
    //OverlappedEx* ov_ex = new OverlappedEx(OperationType::WRITE);
    void* mem = valStore_.writer_str(nullptr, sizeof(OverlappedEx));
    if(mem==nullptr){
        // 分配失败，处理错误
        close();
        return;
    }
    OverlappedEx* ov_ex = new (mem) OverlappedEx(OperationType::WRITE);
    if (ov_ex == nullptr) {
        std::cerr << "Failed to allocate memory for OverlappedEx\n";
        close();
        return;
    }
    std::deque<RPCResponse>load;
    {
        std::lock_guard<std::mutex> lock(mx_dqe1_);
        std::swap(load,dqe1_);
    }
    int n;
    std::string ans;
    while(!dqe1_.empty()&&n<4096){
        std::string t = encoder_.encode(dqe1_.front());
        ans+=t;
        n+=t.size();
        dqe1_.pop_front();
    }
    if(ov_ex==nullptr){
        return ;
    }
    if (!ov_ex) {
        close();
        return;
    }
    if (n > sizeof(ov_ex->buffer)) { 
        //通常消息不会太长
        std::cout<<"消息太长\n";
    }
    memcpy(ov_ex->buffer, ans.data(), n);
    WSABUF buf;
    buf.buf = ov_ex->buffer;
    buf.len = n; 

    DWORD bytesSent = 0; 
    int ret = WSASend(socket_, &buf, 1, &bytesSent, 0, &ov_ex->overlapped, nullptr);

    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            std::cerr << "WSASend failed for socket " << socket_ << " with error: " << err << "\n";
            delete ov_ex;
            close();
        }
    }
}
 void Connection::addRPCResponse(RPCResponse& mess){
    {
        std::lock_guard<std::mutex> lock(mx_dqe1_);
        dqe1_.emplace_back(mess);
    }
 }


void Connection::start(){
    postRecv();
}
void Connection::close(){
    if(alive_.load(std::memory_order_acquire)){
        alive_.store(false,std::memory_order_release);
        closesocket(socket_);
    }else{
        std::cout<<"Connection already closed."<<std::endl;
    }
}
int Connection::getId() const{
    return id_;
}
SOCKET Connection::getSocket() const{
    return socket_;
}
std::atomic<bool> Connection::isAlive() const{
    return alive_.load(std::memory_order_relaxed);
}
HANDLE Connection::getiocp() const{
    return iocp_;
}
bool Connection::dequeEmpty()const{
    return dqe_.empty();
}


Connection::Connection(int id, SOCKET sock, HANDLE iocp, RequestQueue& request_queue,Decoder& decoder,Encoder& encoder,ValueStore& valStore):id_(id), socket_(sock), iocp_(iocp), request_queue_(request_queue),
      decoder_(decoder), 
      alive_(true) ,encoder_(encoder),valStore_(valStore)
{}
Connection::~Connection(){
    close();
}


        // ULONG_PTR keys = reinterpret_cast<ULONG_PTR>(conne);
        // OverlappedEx* ov = new OverlappedEx(OperationType::WRITE);
        // memcpy(ov->buffer,ans.c_str(),ans.size());
        // LPOVERLAPPED  ov_send = reinterpret_cast<LPOVERLAPPED>(ov);
        // BOOL result = PostQueuedCompletionStatus(conne->getiocp(), bytes, keys,ov_send);
        // t.clientFd->postSend(ov,ans.size());
        // LPOVERLAPPED ov_buffer = reinterpret_cast<LPOVERLAPPED>(new OverlappedEx(OperationType::QUEUE_SPACE_AVAILABLE));
        // if(!conne->dequeEmpty())
        //     PostQueuedCompletionStatus(conne->getiocp(),0,keys,ov_buffer);