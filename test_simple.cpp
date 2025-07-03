#include "include/http_parse/simple_parser.hpp"
#include "include/http_parse/simple_encoder.hpp"
#include <iostream>
#include <iomanip>
#include <cstdint>

using namespace co::http;

void print_hex(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0 && i % 16 == 0) std::cout << "\n";
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "HTTP Parse Library - Encoding Test\n";
    std::cout << "==================================\n\n";
    
    // Test 1: HTTP/1.1 request encoding using builder
    std::cout << "1. Building and encoding HTTP/1.1 request:\n";
    auto req = request_builder()
        .GET("/api/users")
        .host("example.com")
        .user_agent("http_parse/1.0")
        .content_type("application/json")
        .build();
    
    encoder enc;
    auto encoded_req = enc.encode_request(req);
    
    std::cout << "Encoded request:\n";
    std::cout << encoded_req << std::endl;
    
    // Test 2: HTTP/1.1 response encoding using builder
    std::cout << "2. Building and encoding HTTP/1.1 response:\n";
    auto resp = response_builder()
        .OK()
        .content_type("application/json")
        .server("http_parse/1.0")
        .body("{\"users\": []}")
        .build();
    
    auto encoded_resp = enc.encode_response(resp);
    
    std::cout << "Encoded response:\n";
    std::cout << encoded_resp << std::endl;
    
    // Test 3: HTTP/2 data frame encoding
    std::cout << "3. Encoding HTTP/2 data frame:\n";
    auto h2_data_frame = enc.encode_h2_data_frame(1, "{\"users\": []}", true);
    
    std::cout << "HTTP/2 data frame bytes (" << h2_data_frame.size() << " bytes):\n";
    print_hex(h2_data_frame);
    std::cout << std::endl;
    
    // Test 4: Connection preface
    std::cout << "4. HTTP/2 connection preface:\n";
    auto preface = enc.get_h2_connection_preface();
    
    std::cout << "Preface bytes:\n";
    for (size_t i = 0; i < preface.size(); ++i) {
        char c = static_cast<char>(preface[i]);
        if (std::isprint(c)) {
            std::cout << c;
        } else {
            std::cout << "\\x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(preface[i]) << std::dec;
        }
    }
    std::cout << std::endl;
    
    std::cout << "\n✓ Encoding tests completed!\n";
    return 0;
}