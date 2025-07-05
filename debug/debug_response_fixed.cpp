#include "http_parse.hpp"
#include <iostream>

using namespace co::http;

int main() {
    response resp;
    resp.protocol_version = version::http_1_1;
    resp.status_code = 200; // OK
    resp.reason_phrase = "OK";
    resp.add_header("content-type", "application/json");
    resp.add_header("server", "TestServer/1.0");
    resp.body = R"({"status": "ok"})";
    
    // Let the encoder calculate content-length automatically
    // Don't set content-length manually
    
    std::cout << "Body length: " << resp.body.length() << std::endl;
    
    auto encoded = http1::encode_response(resp);
    if (encoded.has_value()) {
        std::cout << "Encoded response:" << std::endl << encoded.value() << std::endl;
        
        auto parsed = http1::parse_response(encoded.value());
        if (parsed.has_value()) {
            std::cout << "Parse success!" << std::endl;
            std::cout << "Parsed status: " << parsed.value().status_code << std::endl;
            std::cout << "Parsed body: " << parsed.value().body << std::endl;
        } else {
            std::cout << "Parse failed with error code: " << static_cast<int>(parsed.error()) << std::endl;
        }
    } else {
        std::cout << "Encoding failed!" << std::endl;
    }
    
    return 0;
}