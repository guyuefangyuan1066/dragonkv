#include"command_dispatcher.hpp"
#include"../storage/engine.hpp"
#include <iostream>

CommandDispatcher::CommandDispatcher(StorageEngine& engine)
    : engine_(engine) {}

RPCResponse CommandDispatcher::dispatch(const RPCRequest& request) {
    RPCResponse response;

    switch (request.type) {
    case CommandType::PUT: {
        bool ok = engine_.put(request.key, request.value.value_or(""));
        response.success = ok;
        response.message = ok ? "PUT success" : "PUT failed";
        break;
    }
    case CommandType::GET: {
        auto result = engine_.get(request.key);
        response.success = result.has_value();
        response.value = result;
        response.message = response.success ? "GET success" : "Key not found";
        break;
    }
    case CommandType::REMOVE: {
        bool ok = engine_.remove(request.key);
        response.success = ok;
        response.message = ok ? "REMOVE success" : "REMOVE failed";
        break;
    }
    case CommandType::UNKNOWN: {
        response.success = false;
        response.message = "Unknown command type";
        std::cout << "TYPE IS UNKNOWN\n";
        break;
    }
    default: {
        response.success = false;
        response.message = "Invalid command";
        std::cout << "NO IN CommandType\n";
        break;
    }
    }

    return response;
}
