#include "Decoder.hpp" // 包含 Decoder 类的声明

// 为 CommandType 重载 operator<< 的实现
std::ostream& operator<<(std::ostream& os, CommandType type) {
    switch (type) {
        case CommandType::GET:
            os << "GET";
            break;
        case CommandType::PUT:
            os << "PUT";
            break;
        case CommandType::REMOVE:
            os << "REMOVE";
            break;
        case CommandType::UNKNOWN:
            os << "UNKNOWN";
            break;
        default:
            os << "UNDEFINED_COMMAND";
            break;
    }
    return os;
}

// Decoder::createRPCRequest 的实现
std::optional<RPCRequest> Decoder::createRPCRequest(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::nullopt; // 无效请求
    }

    RPCRequest request;
    std::string command = args[0]; // 命令通常是第一个参数

    // 将命令字符串映射到 CommandType
    if (command == "PUT") {
        request.type = CommandType::PUT;
        if (args.size() != 3) return std::nullopt; // PUT key value
        request.key = args[1];
        request.value = args[2];
    } else if (command == "GET") {
        request.type = CommandType::GET;
        if (args.size() != 2) return std::nullopt; // GET key
        request.key = args[1];
        // GET 请求没有 value，所以 value 保持 std::nullopt
    } else if (command == "REMOVE") {
        request.type = CommandType::REMOVE;
        if (args.size() != 2) return std::nullopt; // REMOVE key
        request.key = args[1];
    } else {
        request.type = CommandType::UNKNOWN;
    }
    return request;
}

// Decoder::parseInteger 的实现
std::optional<int> Decoder::parseInteger(const char* data, size_t len, size_t& current_pos) {
    size_t start_pos = current_pos;
    int value = 0;
    bool negative = false;
    if (current_pos < len && data[current_pos] == '-') {
        negative = true;
        current_pos++;
    }
    bool digit_found = false;
    while (current_pos < len && data[current_pos] >= '0' && data[current_pos] <= '9') {
        value = value * 10 + (data[current_pos] - '0');
        current_pos++;
        digit_found = true;
    }
    if (current_pos >= len || data[current_pos] != '\r' || (current_pos + 1 >= len || data[current_pos+1] != '\n')) {
        current_pos = start_pos; // 回退，表示未成功解析或需要更多数据
        return std::nullopt;
    }
    current_pos += 2; // Consume \r\n
    if (!digit_found && !negative) return std::nullopt; // 空数字或者只有负号
    return negative ? -value : value;
}

// Decoder::parseBulkString 的实现
std::optional<std::string> Decoder::parseBulkString(const char* data, size_t len, size_t& current_pos) {
    size_t start_pos = current_pos;
    if (current_pos >= len || data[current_pos] != '$') {
        return std::nullopt; // 格式错误或数据不足
    }
    current_pos++;

    size_t len_parse_start = current_pos;
    std::optional<int> bulkLenOpt = parseInteger(data, len, current_pos);
    if (!bulkLenOpt) {
        current_pos = start_pos; // 回退
        return std::nullopt; // 长度解析失败或数据不足
    }
    int bulkLen = *bulkLenOpt;

    if (bulkLen < 0) { // RESP Null Bulk String ($-1\r\n)
        return std::string(); // 通常解析为空字符串，或根据协议约定返回 std::nullopt
    }

    // 检查是否有足够的空间来读取整个字符串和结尾的 \r\n
    if (current_pos + bulkLen >= len || 
        current_pos + bulkLen + 1 >= len || // 确保有足够空间读取 \r\n
        data[current_pos + bulkLen] != '\r' || 
        data[current_pos + bulkLen + 1] != '\n') {
        
        current_pos = start_pos; // 数据不足以包含整个字符串和最后的 \r\n，回退
        return std::nullopt;
    }

    std::string result(data + current_pos, bulkLen);
    current_pos += bulkLen + 2; // Advance past string data and \r\n
    return result;
}

// Decoder::parseSingleRequest 的实现
DecodeResult Decoder::parseSingleRequest(const char* data, size_t len, size_t& current_pos_in_buffer) {
    size_t initial_pos = current_pos_in_buffer; // 记录本次解析的起始位置

    // 检查起始字符是否为 '*'，并处理可能的 INCOMPLETE/ERROR
    if (current_pos_in_buffer >= len || data[current_pos_in_buffer] != '*') {
        // 如果当前位置没有数据，或者不是 '*'，且之前没有进行任何解析尝试，则可能是 INCOMPLETE
        // 否则，是协议错误
        if (current_pos_in_buffer == len && initial_pos == current_pos_in_buffer) {
            return {DecodeResultType::C_INCOMPLETE, std::nullopt, 0, ""};
        }
        return {DecodeResultType::C_ERROR, std::nullopt, current_pos_in_buffer - initial_pos, "Protocol Error: Expected '*' for array start"};
    }
    current_pos_in_buffer++;

    size_t argCountParseStart = current_pos_in_buffer;
    std::optional<int> argCountOpt = parseInteger(data, len, current_pos_in_buffer);
    if (!argCountOpt) {
        // 如果解析位置没有移动，说明数据不足
        if (current_pos_in_buffer == argCountParseStart) {
            current_pos_in_buffer = initial_pos; // 回退，标记为未处理
            return {DecodeResultType::C_INCOMPLETE, std::nullopt, 0, ""}; // Need more data for arg count
        }
        // 否则是解析错误
        return {DecodeResultType::C_ERROR, std::nullopt, current_pos_in_buffer - initial_pos, "Protocol Error: Invalid argument count format"};
    }
    int argCount = *argCountOpt;
    if (argCount < 0) { // RESP arrays can be null (e.g. *-1\r\n) or empty (*0\r\n)
        // 对于 RPC 请求，通常我们不处理负数参数计数作为有效请求，但根据协议可能需要特殊处理
        return {DecodeResultType::C_ERROR, std::nullopt, current_pos_in_buffer - initial_pos, "Protocol Error: Negative argument count"};
    }

    std::vector<std::string> args;
    args.reserve(argCount);

    for (int i = 0; i < argCount; ++i) {
        size_t argParseStart = current_pos_in_buffer;
        std::optional<std::string> argOpt = parseBulkString(data, len, current_pos_in_buffer);
        if (!argOpt) {
            // 如果解析位置没有移动，说明数据不足
            if (current_pos_in_buffer == argParseStart) {
                current_pos_in_buffer = initial_pos; // 回退，标记为未处理
                return {DecodeResultType::C_INCOMPLETE, std::nullopt, 0, ""}; // Need more data for bulk string
            }
            // 否则是解析错误
            return {DecodeResultType::C_ERROR, std::nullopt, current_pos_in_buffer - initial_pos, "Protocol Error: Invalid bulk string format"};
        }
        args.push_back(std::move(*argOpt));
    }

    std::optional<RPCRequest> request = createRPCRequest(args);
    if (!request) {
        return {DecodeResultType::C_ERROR, std::nullopt, current_pos_in_buffer - initial_pos, "Protocol Error: Invalid command or arguments for RPCRequest"};
    }

    return {DecodeResultType::C_OK, request, current_pos_in_buffer - initial_pos, ""};
}

// Decoder::appendData 的实现
void Decoder::appendData(const char* data, size_t len) {
    if (data && len > 0) {
        buffer_.insert(buffer_.end(), data, data + len);
    }
}

// Decoder::parseAllAvailableRequests 的实现
std::vector<RPCRequest> Decoder::parseAllAvailableRequests() {
    std::vector<RPCRequest> parsedRequests;
    size_t current_parse_offset = 0; // 当前在 buffer_ 中的解析偏移量

    while (true) {
        // 如果缓冲区为空，或者没有足够的数据来判断是否是一个新消息的开始，就停止
        if (current_parse_offset >= buffer_.size()) {
             break;
        }

        // 尝试解析一个完整的 RPC 请求
        // 注意：这里传递的是 buffer_ 的起始地址，并加上当前偏移量
        // 同时，传入的长度是 buffer_ 中从当前偏移量开始的剩余数据长度
        DecodeResult result = parseSingleRequest(buffer_.data(), buffer_.size(), current_parse_offset);
        
        if (result.type == DecodeResultType::C_OK) {
            if (result.request) {
                parsedRequests.push_back(std::move(*result.request));
            }
            // current_parse_offset 已经被 parseSingleRequest 更新到当前消息的末尾
            // 继续循环，尝试解析下一个消息
        } else if (result.type == DecodeResultType::C_INCOMPLETE) {
            // 当前缓冲区数据不足以形成一个完整的请求，等待更多数据
            // current_parse_offset 在 parseSingleRequest 中已经回退到解析开始位置
            break; 
        } else { // DecodeResultType::ERROR
            // 遇到协议错误，通常需要清空缓冲区并通知调用者（这里通过 std::cerr）
            std::cerr << "Decoding Error: " << result.errorMessage << std::endl;
            buffer_.clear(); // 清空所有数据，避免错误蔓延
            current_parse_offset = 0; // 重置偏移量
            break; // 遇到错误就停止当前批次的解析
        }
    }
    
    // 在循环结束后，将已成功解析的请求从 buffer_ 的前端移除
    // 只有当解析出至少一个请求或者遇到 INCOMPLETE 时，才会有需要移除的数据
    if (current_parse_offset > 0 && current_parse_offset <= buffer_.size()) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + current_parse_offset);
    } else if (current_parse_offset == 0 && !buffer_.empty() && parsedRequests.empty()) {
        // 如果没有任何请求被解析，但缓冲区不为空，且 parseSingleRequest 返回 INCOMPLETE/ERROR
        // 则不需要擦除，因为数据还留在缓冲区中待后续处理或已清空
    }
    
    return parsedRequests;
}