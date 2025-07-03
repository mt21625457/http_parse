#include "../include/http_parse/http_parse.hpp"
#include <iostream>
#include <array>
#include <vector>

int main() {
    using namespace co::http;
    
    // Create HTTP/2 connection
    v2::connection conn;
    
    // Set up event handlers
    conn.set_on_headers([](v2::stream_id stream_id, const std::vector<v2::header_field>& headers) {
        std::cout << "Stream " << stream_id << " headers received (" << headers.size() << " headers):\n";
        for (const auto& header : headers) {
            std::cout << "  " << header.name << ": " << header.value << "\n";
        }
    });
    
    conn.set_on_data([](v2::stream_id stream_id, std::span<const uint8_t> data) {
        std::cout << "Stream " << stream_id << " data received: " << data.size() << " bytes\n";
        // Print first few bytes as example
        if (data.size() > 0) {
            std::cout << "  First bytes: ";
            for (size_t i = 0; i < std::min(data.size(), size_t(10)); ++i) {
                std::cout << std::hex << static_cast<int>(data[i]) << " ";
            }
            std::cout << std::dec << "\n";
        }
    });
    
    conn.set_on_stream_error([](v2::stream_id stream_id, v2::error_code error) {
        std::cout << "Stream " << stream_id << " error: " << static_cast<int>(error) << "\n";
    });
    
    conn.set_on_connection_error([](v2::error_code error) {
        std::cout << "Connection error: " << static_cast<int>(error) << "\n";
    });
    
    // Simulate HTTP/2 connection preface
    std::array<uint8_t, 24> preface = {
        'P', 'R', 'I', ' ', '*', ' ', 'H', 'T', 'T', 'P', '/', '2', '.', '0', '\r', '\n',
        '\r', '\n', 'S', 'M', '\r', '\n', '\r', '\n'
    };
    
    std::cout << "Processing HTTP/2 connection preface...\n";
    
    // Process the preface
    auto result = conn.process_data(std::span<const uint8_t>(preface.data(), preface.size()));
    if (result) {
        std::cout << "Successfully processed " << *result << " bytes of preface\n";
    } else {
        std::cout << "Failed to process preface\n";
    }
    
    // Show active streams
    auto active_streams = conn.active_streams();
    std::cout << "Active streams: " << active_streams.size() << "\n";
    
    return 0;
}