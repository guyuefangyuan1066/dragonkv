#ifndef COMMAND_DISPATCHER_HPP
#define COMMAND_DISPATCHER_HPP
#include"../network/protocol/rpc_types.hpp"
class StorageEngine;
class CommandDispatcher {
public:
    explicit CommandDispatcher(StorageEngine& engine);
    RPCResponse dispatch(const RPCRequest& request);

private:
    StorageEngine& engine_;
};


#endif