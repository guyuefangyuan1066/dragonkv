#ifndef ENCODER_HPP
#define ENCODER_HPP
#include"rpc_types.hpp"
class Encoder {

    public:
    std::string encode( RPCResponse& response) ;
};
#endif
