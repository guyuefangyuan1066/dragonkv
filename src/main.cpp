//cd bin
//.\dragonkv
//telnet 127.0.0.1 50000

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <optional>
#include <cassert> 
#include <type_traits> 
#include <atomic> // 用于服务器运行状态标志
#include <condition_variable> // 用于主线程阻塞等待
#include <csignal> // 用于处理系统信号 (如 Ctrl+C)

#include"core/MPMCQueue.hpp"
#include"core/reactor.hpp"
#include"core/thread_pool.hpp"
#include"storage/engine.hpp"
#include"app/command_dispatcher.hpp"

// 全局标志和条件变量，用于控制服务器运行和停止
std::atomic<bool> g_server_running(true);
std::mutex g_stop_mutex;
std::condition_variable g_stop_cv;

// 信号处理函数
void signal_handler(int signal) {
    if (signal == SIGINT) { // 捕获 Ctrl+C
        std::cout << "\nSIGINT received. Initiating server shutdown...\n";
        g_server_running.store(false); // 设置停止标志
        g_stop_cv.notify_one(); // 通知主线程停止
    }
}



int main() {
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::cout << "main() begin...\n";
    std::cout << "StorageEngine start...\n";
    StorageEngine* SE = new StorageEngine("../log");
    std::cout << "StorageEngine start over...\n";
    Encoder e;
    Decoder d;
    CommandDispatcher app = CommandDispatcher(*SE);
    std::cout << "RequestQueue start...\n";
    RequestQueue qe;
    std::cout << "thread start...\n";
    IOCPReactor* IO = new IOCPReactor(1, qe, d,e,SE->get_valStore());
    WorkerThreadPool* work = new WorkerThreadPool(8, qe, app,IO->getiocp());
    std::cout << "thread start over...\n";
    IO->start(50000);
    work->start();

    std::cout << "Server started. Press Ctrl+C to stop...\n";

    // 主线程在这里阻塞，直到 g_server_running 变为 false
    {
        std::unique_lock<std::mutex> lock(g_stop_mutex);
        g_stop_cv.wait(lock, [] { return !g_server_running.load(); });
    }

    std::cout << "Stopping server...\n";

    work->stop(); // 通知工作线程池停止，并等待其线程join
    IO->stop();   // 通知网络层停止，并清理连接等

    // 清理动态分配的内存
    delete work;
    delete IO;
    delete SE; 

    std::cout << "Server stopped gracefully.\n";

    return 0;
}