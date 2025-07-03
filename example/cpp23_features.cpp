#include "../include/http_parse/http_parse.hpp"

#if __cplusplus >= 202302L
#include "../include/http_parse/v1/concepts.hpp"
#include "../include/http_parse/v2/concepts.hpp"
#include <iostream>
#include <ranges>

int main() {
    using namespace co::http;
    
    std::cout << "C++23 Features Demo\n";
    std::cout << "==================\n\n";
    
    // 1. String literal operators
    std::cout << "1. String Literal Operators:\n";
    {
        using namespace v1::literals;
        
        auto get_method = "GET"_method;
        auto post_method = "POST"_method;
        auto put_method = "PUT"_method;
        
        std::cout << "   GET method: " << static_cast<int>(get_method) << "\n";
        std::cout << "   POST method: " << static_cast<int>(post_method) << "\n";
        std::cout << "   PUT method: " << static_cast<int>(put_method) << "\n";
    }
    
    // 2. Concepts for type safety
    std::cout << "\n2. Concepts for Type Safety:\n";
    {
        v1::request req;
        req.method = "GET";
        req.uri = "/api/test";
        req.add_header("content-type", "application/json");
        req.add_header("authorization", "Bearer token123");
        
        // Function using concepts
        auto process_message = []<v1::http_message_like T>(T& msg) {
            std::cout << "   Processing message with " << msg.headers.size() << " headers\n";
            
            if (auto content_type = msg.get_header("content-type"); content_type) {
                std::cout << "   Content-Type: " << *content_type << "\n";
            }
        };
        
        process_message(req);
    }
    
    // 3. Enhanced header access
    std::cout << "\n3. Enhanced Header Access:\n";
    {
        v1::request req;
        req.add_header("host", "example.com");
        req.add_header("user-agent", "MyClient/1.0");
        
        v1::header_accessor accessor(req);
        
        // Modern subscript access
        if (auto host = accessor["host"]; host) {
            std::cout << "   Host: " << *host << "\n";
        }
        
        if (auto user_agent = accessor["user-agent"]; user_agent) {
            std::cout << "   User-Agent: " << *user_agent << "\n";
        }
        
        // Add new header using modern syntax
        accessor["content-length", "1024"];
        
        std::cout << "   Total headers after adding: " << req.headers.size() << "\n";
    }
    
    // 4. Pattern matching for HTTP methods
    std::cout << "\n4. Pattern Matching for Methods:\n";
    {
        using namespace v1::literals;
        
        auto classify_method = [](v1::method m) -> std::string_view {
            switch (m) {
                case v1::method::get:
                case v1::method::head:
                case v1::method::options:
                    return "Safe method";
                case v1::method::post:
                case v1::method::patch:
                    return "Unsafe method";
                case v1::method::put:
                case v1::method::delete_:
                    return "Idempotent method";
                default:
                    return "Unknown method";
            }
        };
        
        std::cout << "   GET: " << classify_method("GET"_method) << "\n";
        std::cout << "   POST: " << classify_method("POST"_method) << "\n";
        std::cout << "   PUT: " << classify_method("PUT"_method) << "\n";
        std::cout << "   DELETE: " << classify_method("DELETE"_method) << "\n";
    }
    
    // 5. Ranges for header processing
    std::cout << "\n5. Ranges for Header Processing:\n";
    {
        v1::request req;
        req.add_header("accept", "text/html");
        req.add_header("accept-encoding", "gzip, deflate");
        req.add_header("accept-language", "en-US,en;q=0.9");
        req.add_header("cache-control", "no-cache");
        
        // Use ranges to filter headers
        auto accept_headers = req.headers | 
            std::views::filter([](const auto& header) {
                return header.name.starts_with("accept");
            });
        
        std::cout << "   Accept-related headers:\n";
        for (const auto& header : accept_headers) {
            std::cout << "     " << header.name << ": " << header.value << "\n";
        }
    }
    
    // 6. HTTP/2 structured bindings
    std::cout << "\n6. HTTP/2 Structured Bindings:\n";
    {
        v2::frame_header header;
        header.length = 1024;
        header.type = static_cast<uint8_t>(v2::frame_type::data);
        header.flags = static_cast<uint8_t>(v2::frame_flags::end_stream);
        header.stream_id = 1;
        
        v2::frame_header_view view(header);
        
        std::cout << "   Frame - Length: " << view.length 
                  << ", Type: " << static_cast<int>(view.type)
                  << ", Flags: " << static_cast<int>(view.flags)
                  << ", Stream ID: " << view.stream_id << "\n";
    }
    
    std::cout << "\n✓ C++23 features demonstration complete!\n";
    return 0;
}

#else

#include <iostream>

int main() {
    std::cout << "This example requires C++23 support.\n";
    std::cout << "Current C++ version: " << __cplusplus << "\n";
    std::cout << "Required C++ version: 202302L or higher\n";
    return 1;
}

#endif