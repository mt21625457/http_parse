#include "include/http_parse.hpp"
#include <iostream>

int main() {
    try {
        // Test basic compilation
        co::http::request req;
        req.method_type = co::http::method::get;
        req.target = "/test";
        req.body = "test body";
        req.headers.push_back({"Content-Type", "application/json"});
        
        co::http::response resp;
        resp.status_code = 200;
        resp.reason_phrase = "OK";
        resp.body = "response body";
        resp.headers.push_back({"Content-Type", "application/json"});
        
        // Test buffer operations
        co::http::output_buffer buffer;
        buffer.append("test");
        buffer.append_byte(65); // 'A'
        
        std::string result = buffer.to_string();
        
        std::cout << "✓ Core compilation successful!" << std::endl;
        std::cout << "✓ Request method: " << static_cast<int>(req.method_type) << std::endl;
        std::cout << "✓ Request target: " << req.target << std::endl;
        std::cout << "✓ Response status: " << resp.status_code << std::endl;
        std::cout << "✓ Buffer result: " << result << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }
}