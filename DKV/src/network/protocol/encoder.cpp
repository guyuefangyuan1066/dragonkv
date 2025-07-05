
#include"encoder.hpp"


std::string Encoder::encode( RPCResponse& response) {
    if (response.value.has_value()) {
        return "$" + std::to_string(response.value->size()) + "\r\n" +
            response.value.value() + "\r\n";
    } else {
        if (response.success) {
            return "+OK\r\n";
        } else {
            return "-ERR " + response.message + "\r\n";
        }
    }
}


