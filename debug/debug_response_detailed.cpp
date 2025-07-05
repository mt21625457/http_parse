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
    resp.add_header("content-length", "17");
    resp.body = R"({"status": "ok"})";
    
    auto encoded = http1::encode_response(resp);
    if (encoded.has_value()) {
        std::cout << "Encoded response (escaped):" << std::endl;
        std::string escaped = encoded.value();
        for (char c : escaped) {
            if (c == '\r') std::cout << "\\r";
            else if (c == '\n') std::cout << "\\n";
            else std::cout << c;
        }
        std::cout << std::endl << std::endl;
        
        std::cout << "Trying to parse this response:" << std::endl;
        
        // Try using the direct parser function for debugging
        #include "http_parse/v1/parser_impl.hpp"
        auto parsed_direct = co::http::v1::detail::parse_http1_response(encoded.value());
        if (parsed_direct.has_value()) {
            std::cout << "Direct parse success!" << std::endl;
        } else {
            std::cout << "Direct parse failed with error code: " << static_cast<int>(parsed_direct.error()) << std::endl;
        }
        
        // Try the high-level function
        auto parsed = http1::parse_response(encoded.value());
        if (parsed.has_value()) {
            std::cout << "High-level parse success!" << std::endl;
        } else {
            std::cout << "High-level parse failed with error code: " << static_cast<int>(parsed.error()) << std::endl;
        }
    } else {
        std::cout << "Encoding failed!" << std::endl;
    }
    
    return 0;
}