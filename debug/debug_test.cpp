#include "http_parse.hpp"
#include <iostream>

using namespace co::http;

int main() {
    request req;
    req.method_type = method::get;
    req.target = "/api/users";
    req.protocol_version = version::http_1_1;
    req.add_header("host", "api.example.com");
    
    std::cout << "Request target: '" << req.target << "'" << std::endl;
    std::cout << "Request uri: '" << req.uri << "'" << std::endl;
    
    auto result = http1::encode_request(req);
    if (result.has_value()) {
        std::cout << "Encoded result:\n" << result.value() << std::endl;
    } else {
        std::cout << "Encoding failed" << std::endl;
    }
    
    return 0;
}