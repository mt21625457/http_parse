#include "../include/http_parse/http_parse.hpp"
#include <iostream>
#include <string_view>

int main() {
    using namespace co::http;
    
    // Test data for different HTTP versions
    std::string_view http1_data = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Authorization: Bearer token123\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    
    std::string_view http2_preface = 
        "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    
    // Create parser with auto-detection
    parser p(version::auto_detect);
    
    std::cout << "Testing HTTP version auto-detection...\n\n";
    
    // Test 1: HTTP/1.1 request
    std::cout << "1. Testing HTTP/1.1 request:\n";
    request req1;
    auto result1 = p.parse_request(
        std::span<const char>(http1_data.data(), http1_data.size()), 
        req1
    );
    
    if (result1) {
        std::cout << "   ✓ Successfully parsed as HTTP/1.1\n";
        std::cout << "   Detected version: " << 
            (p.detected_version() == version::http_1_1 ? "HTTP/1.1" : "Other") << "\n";
        std::cout << "   Method: " << req1.method << "\n";
        std::cout << "   URI: " << req1.uri << "\n";
        std::cout << "   Headers: " << req1.headers.size() << "\n";
    } else {
        std::cout << "   ✗ Failed to parse HTTP/1.1 request\n";
    }
    
    // Reset parser for next test
    p.reset();
    
    // Test 2: HTTP/2 preface
    std::cout << "\n2. Testing HTTP/2 preface:\n";
    request req2;
    auto result2 = p.parse_request(
        std::span<const char>(http2_preface.data(), http2_preface.size()), 
        req2
    );
    
    if (result2) {
        std::cout << "   ✓ Successfully processed HTTP/2 preface\n";
        std::cout << "   Detected version: " << 
            (p.detected_version() == version::http_2_0 ? "HTTP/2.0" : "Other") << "\n";
    } else {
        std::cout << "   ✗ Failed to process HTTP/2 preface\n";
    }
    
    // Test 3: Manual version selection
    std::cout << "\n3. Testing manual version selection:\n";
    
    // Force HTTP/1.1 parsing
    auto result3 = parse_request(http1_data, version::http_1_1);
    if (result3) {
        std::cout << "   ✓ HTTP/1.1 forced parsing successful\n";
        std::cout << "   Method: " << result3->method << "\n";
        std::cout << "   URI: " << result3->uri << "\n";
    }
    
    // Test invalid data
    std::cout << "\n4. Testing invalid data:\n";
    std::string_view invalid_data = "INVALID REQUEST DATA";
    auto result4 = parse_request(invalid_data, version::auto_detect);
    if (!result4) {
        std::cout << "   ✓ Correctly rejected invalid data\n";
    } else {
        std::cout << "   ✗ Unexpectedly accepted invalid data\n";
    }
    
    return 0;
}