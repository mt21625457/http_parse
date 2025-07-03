#include "../include/http_parse/http_parse.hpp"
#include <iostream>
#include <vector>
#include <string>

int main() {
    using namespace co::http;
    
    std::cout << "Incremental Parsing Demo\n";
    std::cout << "=======================\n\n";
    
    // Simulate receiving HTTP data in chunks
    std::vector<std::string> chunks = {
        "GET /api/users HTTP/1.1\r\n",
        "Host: api.example.com\r\n",
        "User-Agent: MyClient/1.0\r\n",
        "Content-Type: application/json\r\n",
        "Content-Length: 27\r\n",
        "\r\n",
        "{\"name\":\"John\",\"age\":30}"
    };
    
    // Create parser
    v1::parser parser;
    v1::request request;
    
    std::string accumulated_data;
    
    std::cout << "Processing HTTP request in chunks:\n";
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        accumulated_data += chunks[i];
        
        std::cout << "\nChunk " << (i + 1) << ": \"" << chunks[i] << "\"\n";
        std::cout << "Accumulated data size: " << accumulated_data.size() << " bytes\n";
        
        // Try to parse the accumulated data
        auto result = parser.parse_request(
            std::span<const char>(accumulated_data.data(), accumulated_data.size()),
            request
        );
        
        if (result) {
            std::cout << "✓ Parsed " << *result << " bytes\n";
            
            if (parser.is_complete()) {
                std::cout << "✓ Request parsing complete!\n";
                break;
            } else {
                std::cout << "◦ Partial parse - need more data\n";
            }
        } else {
            // Check if we need more data
            if (result.error() == v1::error_code::need_more_data) {
                std::cout << "◦ Need more data to continue parsing\n";
            } else {
                std::cout << "✗ Parse error: " << static_cast<int>(result.error()) << "\n";
                break;
            }
        }
    }
    
    // Display final parsed request
    if (parser.is_complete()) {
        std::cout << "\n" << std::string(40, '=') << "\n";
        std::cout << "Final Parsed Request:\n";
        std::cout << std::string(40, '=') << "\n";
        std::cout << "Method: " << static_cast<int>(request.method_val) << "\n";
        std::cout << "URI: " << request.uri << "\n";
        std::cout << "Version: " << request.version << "\n";
        std::cout << "Headers (" << request.headers.size() << "):\n";
        
        for (const auto& header : request.headers) {
            std::cout << "  " << header.name << ": " << header.value << "\n";
        }
        
        std::cout << "Body length: " << request.body.size() << " bytes\n";
        if (!request.body.empty()) {
            std::cout << "Body: " << request.body << "\n";
        }
    }
    
    // Demonstrate parser reset
    std::cout << "\n" << std::string(40, '-') << "\n";
    std::cout << "Testing parser reset:\n";
    parser.reset();
    
    std::cout << "Parser reset - is_complete(): " << 
        (parser.is_complete() ? "true" : "false") << "\n";
    
    // Test with a simple GET request
    std::string simple_request = "GET / HTTP/1.1\r\n\r\n";
    v1::request simple_req;
    
    auto simple_result = parser.parse_request(
        std::span<const char>(simple_request.data(), simple_request.size()),
        simple_req
    );
    
    if (simple_result && parser.is_complete()) {
        std::cout << "✓ Successfully parsed simple request\n";
        std::cout << "  Method: " << static_cast<int>(simple_req.method_val) << "\n";
        std::cout << "  URI: " << simple_req.uri << "\n";
        std::cout << "  Headers: " << simple_req.headers.size() << "\n";
    }
    
    return 0;
}