#include "../include/http_parse/http_parse.hpp"
#include <iostream>
#include <string_view>

int main() {
    using namespace co::http;
    
    // Example HTTP/1.1 request
    std::string_view request_data = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "Accept: text/html\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    // Parse the request
    auto result = parse_request(request_data, version::http_1_1);
    if (result) {
        const auto& req = *result;
        std::cout << "Successfully parsed HTTP/1.1 request:\n";
        std::cout << "Method: " << req.method << "\n";
        std::cout << "URI: " << req.uri << "\n";
        
        // Access headers
        if (auto host = req.get_header("host"); host) {
            std::cout << "Host: " << *host << "\n";
        }
        
        if (auto user_agent = req.get_header("user-agent"); user_agent) {
            std::cout << "User-Agent: " << *user_agent << "\n";
        }
        
        std::cout << "Headers count: " << req.headers.size() << "\n";
    } else {
        std::cout << "Failed to parse request\n";
    }
    
    return 0;
}