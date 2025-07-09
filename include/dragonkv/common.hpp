#ifndef DRAGONKV_COMMON_HPP
#define DRAGONKV_COMMON_HPP

#include<string>
#include <mswsock.h>
#include <windows.h>
#include <cstdint>
#include"../../src/core/MPMCQueue.hpp"
#include"../../src/network/protocol/rpc_types.hpp"
struct ValueMeta {
    void* dataPtr;     
    size_t length;     
    uint64_t timestamp;
};

enum class CommandType {
    PUT,
    GET,
    REMOVE,
    UNKNOWN
};

// RPCRequest 结构体
struct RPCRequest {
    CommandType type;
    std::string key;
    std::optional<std::string> value; // GET/REMOVE 请求可能没有 value
};
struct RPCResponse {
    bool success;
    std::optional<std::string> value;  // only for GET
    std::string message;               // 用于错误描述等
};
class Connection;
struct RequestMessage {
    Connection* clientFd;                         // 标识连接
    RPCRequest request;
};
using RequestQueue = MPMCQueue<RequestMessage, 1024>;
using walQueue = MPMCQueue<std::string, 1024>;
enum class OperationType { READ, WRITE,QUEUE_SPACE_AVAILABLE };
struct WriteContext {
    OVERLAPPED overlapped{};
    char* buffer;
    HANDLE file;
};








// 为 CommandType 重载 operator<<，方便打印（通常放在头文件中）
std::ostream& operator<<(std::ostream& os, CommandType type);




//enum class OperationType { READ, WRITE };

    struct OverlappedEx {
        OVERLAPPED overlapped;
        OperationType type;
        char buffer[2048];

        OverlappedEx(OperationType t) : type(t) {
            memset(&overlapped, 0, sizeof(overlapped));
            memset(buffer, 0, sizeof(buffer));
        }
    };


#endif
