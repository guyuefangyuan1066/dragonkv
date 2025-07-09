# DragonKV

DragonKV 是一个基于 C++17 的高性能嵌入式键值数据库，支持多线程并发、IOCP 网络模型和自定义内存池。

## 主要特性
- 多线程高并发请求处理（线程池+无锁队列）
- IOCP（I/O 完成端口）高效网络通信
- 自定义对象池与 value pool 内存管理
- 跳表索引与分片并发控制
- RESP 协议兼容，支持 telnet/自定义客户端
- 支持 PUT/GET/REMOVE 基本操作
- 性能测试与单元测试用例

## 目录结构
```
DKV/
├── bin/                # 可执行文件与性能报告
├── include/            # 公共头文件
├── src/                # 源码目录
│   ├── app/            # 命令分发
│   ├── core/           # 线程池、无锁队列、reactor
│   ├── memory/         # 内存池、value store
│   ├── network/        # 网络协议、连接管理
│   └── storage/        # 存储引擎、索引、WAL
├── tests/              # 测试代码
│   └── mem/            # 性能测试
└── ...
```

## 编译方法

### 依赖
- C++17 编译器（推荐 MinGW-w64 或 MSVC）
- Windows 平台（使用 IOCP）

### 编译主服务
在项目根目录下：
```sh
# Windows 下（MinGW）
g++ -std=c++17 -O2 -I./include -I./src -o bin/dragonkv.exe \
    src/main.cpp src/core/thread_pool.cpp src/core/reactor.cpp \
    src/memory/value_store.cpp src/memory/object_pool.cpp \
    src/network/connection.cpp src/network/protocol/decoder.cpp src/network/protocol/encoder.cpp \
    src/storage/engine.cpp src/storage/index/index_manager.cpp src/storage/index/skip_list.cpp \
    src/storage/wal/wal_writer.cpp src/app/command_dispatcher.cpp -lws2_32
```

### 编译性能测试
在 `tests/mem` 目录下：
```sh
cd tests/mem
g++ -std=c++17 -O2 -I../../src -I../../src/storage -I../../src/storage/index -I../../src/memory -I../../src/core -I../../src/app \
    -o tests_mem tests_mem.cpp \
    ../../src/storage/engine.cpp ../../src/storage/index/index_manager.cpp ../../src/storage/index/skip_list.cpp \
    ../../src/memory/value_store.cpp ../../src/memory/object_pool.cpp \
    ../../src/core/thread_pool.cpp ../../src/core/reactor.cpp ../../src/app/command_dispatcher.cpp
```

## 运行方法

### 启动服务
```sh
cd bin
./dragonkv.exe
```

### 客户端测试
- 可用 telnet 连接：
  ```sh
  telnet 127.0.0.1 50000
  ```
- 或使用 `tests/shortconn_client.cpp` 编译运行进行自动化测试。

### 性能测试
```sh
cd tests/mem
./tests_mem
```

## 主要模块说明
- `src/core/MPMCQueue.hpp`：无锁多生产多消费队列
- `src/core/thread_pool.cpp`：工作线程池
- `src/core/reactor.cpp`：IOCP 网络反应器
- `src/memory/value_store.cpp`：自定义 value pool
- `src/storage/engine.cpp`：存储引擎
- `src/storage/index/index_manager.cpp`：分片跳表索引
- `src/network/connection.cpp`：网络连接与协议处理

## 贡献与许可
本项目仅供学习与研究使用，欢迎 issue 和 PR。
