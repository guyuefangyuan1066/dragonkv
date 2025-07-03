#ifndef DECODER_HPP
#define DECODER_HPP

#include <vector>
#include <string>
#include <optional>
#include <iostream> // For std::ostream and std::cerr (can be moved to .cpp if preferred for logging)
#include <algorithm> // For std::min
#include"../../../include/dragonkv/common.hpp"
// 定义 CommandType 枚举


// DecodeResultType 枚举，表示解码结果类型
enum class DecodeResultType {
    C_OK,         // 成功解码一个完整请求
    C_INCOMPLETE, // 数据不足，需要更多数据
    C_ERROR       // 解码过程中发生协议错误
};

// DecodeResult 结构体，表示单次解码尝试的结果
struct DecodeResult {
    DecodeResultType type;
    std::optional<RPCRequest> request; // 如果 type 是 OK，则包含解析出的请求
    size_t bytesProcessed;             // 在无状态版本中表示处理了多少字节，有状态版本中通常无需外部感知
    std::string errorMessage;          // 如果 type 是 ERROR，则包含错误信息
};


// Decoder 类声明
class Decoder {
private:
    std::vector<char> buffer_; // 内部缓冲区，存储未处理的网络数据

    // 辅助函数：将字符串命令映射到 CommandType 并构建 RPCRequest
    std::optional<RPCRequest> createRPCRequest(const std::vector<std::string>& args);

    // 辅助函数：解析 RESP 整数
    // data: 指向当前解析数据的起始指针
    // len: 可用数据的总长度
    // current_pos: 输入时表示开始解析的位置，输出时表示解析结束后的位置
    std::optional<int> parseInteger(const char* data, size_t len, size_t& current_pos);

    // 辅助函数：解析 RESP Bulk String
    // data: 指向当前解析数据的起始指针
    // len: 可用数据的总长度
    // current_pos: 输入时表示开始解析的位置，输出时表示解析结束后的位置
    std::optional<std::string> parseBulkString(const char* data, size_t len, size_t& current_pos);

    // 内部 helper 函数，只尝试解析一个完整的 RESP 请求
    // 这里的 data 是整个 buffer_.data()，len 是 buffer_.size()
    // current_pos_in_buffer: 引用类型，函数内部会更新此值
    DecodeResult parseSingleRequest(const char* data, size_t len, size_t& current_pos_in_buffer);

public:
    // 外部调用此方法将接收到的字节追加到解码器
    void appendData(const char* data, size_t len);

    // 此方法将循环解析，并返回所有解析出的完整RPCRequest
    std::vector<RPCRequest> parseAllAvailableRequests();
};

#endif // DECODER_HPP