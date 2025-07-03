# HTTP Parse Library

A modern C++ header-only library for parsing HTTP/1.x and HTTP/2 protocols, leveraging C++23 features for optimal performance and type safety.

## Features

- **Pure Protocol Parsing**: Header-only library focused on protocol parsing
- **Multi-Version Support**: HTTP/1.0, HTTP/1.1, and HTTP/2
- **Modern C++**: Utilizes C++23 features with fallback for older standards
- **Type Safety**: Extensive use of concepts and strong typing
- **Performance**: Zero-copy parsing where possible
- **Extensible**: Easy to extend with custom frame types and handlers

## Architecture

```
include/http_parse/
├── http_parse.hpp          # Main unified API
├── v1/                     # HTTP/1.x implementation
│   ├── parser.hpp          # HTTP/1.x parser interface
│   ├── parser_impl.hpp     # HTTP/1.x parser implementation
│   └── concepts.hpp        # C++23 concepts for HTTP/1.x
├── v2/                     # HTTP/2 implementation
│   ├── parser.hpp          # HTTP/2 parser interface
│   ├── parser_impl.hpp     # HTTP/2 parser implementation
│   └── concepts.hpp        # C++23 concepts for HTTP/2
└── example.hpp             # Usage examples
```

## Quick Start

### Basic HTTP/1.1 Parsing

```cpp
#include <http_parse/http_parse.hpp>

using namespace co::http;

// Parse a complete HTTP request
std::string_view request_data = 
    "GET /index.html HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: Mozilla/5.0\r\n"
    "\r\n";

auto result = parse_request(request_data);
if (result) {
    const auto& req = *result;
    std::cout << "Method: " << req.method << std::endl;
    std::cout << "URI: " << req.uri << std::endl;
    
    if (auto host = req.get_header("host"); host) {
        std::cout << "Host: " << *host << std::endl;
    }
}
```

### HTTP/2 Connection Handling

```cpp
#include <http_parse/http_parse.hpp>

using namespace co::http;

v2::connection conn;

// Set up event handlers
conn.set_on_headers([](v2::stream_id stream_id, const std::vector<v2::header_field>& headers) {
    std::cout << "Stream " << stream_id << " headers received" << std::endl;
});

conn.set_on_data([](v2::stream_id stream_id, std::span<const uint8_t> data) {
    std::cout << "Stream " << stream_id << " data: " << data.size() << " bytes" << std::endl;
});

// Process incoming data
std::array<uint8_t, 1024> buffer;
// ... fill buffer from network ...
auto result = conn.process_data(std::span<const uint8_t>(buffer));
```

### Auto-detection of HTTP Version

```cpp
#include <http_parse/http_parse.hpp>

using namespace co::http;

parser p(version::auto_detect);
request req;

// The parser will automatically detect HTTP/1.x or HTTP/2
auto result = p.parse_request(data, req);
if (result) {
    std::cout << "Detected version: " << 
        (p.detected_version() == version::http_2_0 ? "HTTP/2" : "HTTP/1.x") << std::endl;
}
```

## C++23 Features

When compiled with C++23 support, the library provides enhanced features:

### String Literal Operators

```cpp
#include <http_parse/v1/concepts.hpp>

using namespace co::http::v1::literals;

auto method = "GET"_method;  // Compile-time method parsing
```

### Concepts for Type Safety

```cpp
#include <http_parse/v1/concepts.hpp>

template<co::http::v1::http_message_like T>
void process_message(T& msg) {
    // Guaranteed to have headers and body
    if (auto content_type = msg.get_header("content-type"); content_type) {
        // Process based on content type
    }
}
```

### Enhanced Header Access

```cpp
#include <http_parse/v1/concepts.hpp>

using namespace co::http::v1;

request req;
header_accessor accessor(req);

// Modern subscript operator
if (auto auth = accessor["authorization"]; auth) {
    std::cout << "Auth: " << *auth << std::endl;
}

// Set headers
accessor["content-type", "application/json"];
```

## Design Principles

1. **Zero Dependencies**: Only depends on standard library
2. **Header-Only**: Easy integration, no separate compilation
3. **Modern C++**: Uses latest language features for safety and performance
4. **Backward Compatibility**: Graceful fallback for older C++ standards
5. **Extensible**: Easy to add new frame types and protocol extensions

## API Reference

### Core Types

- `co::http::version`: HTTP version enumeration
- `co::http::error_code`: Error codes for parsing operations
- `co::http::request`: HTTP request representation
- `co::http::response`: HTTP response representation
- `co::http::parser`: Main parser class with version auto-detection

### HTTP/1.x Specific

- `co::http::v1::method`: HTTP method enumeration
- `co::http::v1::parser`: Dedicated HTTP/1.x parser
- `co::http::v1::header`: Header name-value pair

### HTTP/2 Specific

- `co::http::v2::parser`: Dedicated HTTP/2 parser
- `co::http::v2::connection`: High-level HTTP/2 connection handler
- `co::http::v2::frame_type`: HTTP/2 frame type enumeration
- `co::http::v2::header_field`: HTTP/2 header field with sensitivity flag

## Error Handling

The library uses `std::expected` (C++23) or a compatible implementation for error handling:

```cpp
auto result = parse_request(data);
if (!result) {
    switch (result.error()) {
        case error_code::need_more_data:
            // Wait for more data
            break;
        case error_code::protocol_error:
            // Handle protocol violation
            break;
        // ... other error cases
    }
}
```

## Performance Considerations

- **Zero-copy parsing**: Uses `std::span` and `std::string_view` to avoid data copying
- **Incremental parsing**: Supports partial data processing
- **Memory efficient**: Minimal memory allocations during parsing
- **HPACK compression**: Built-in HTTP/2 header compression support

## Compilation Requirements

- **Minimum**: C++17 (with compatibility shims)
- **Recommended**: C++20 (for concepts and ranges)
- **Optimal**: C++23 (for all advanced features)

## Thread Safety

The library is **not thread-safe** by design. Each parser instance should be used from a single thread. For multi-threaded applications, use separate parser instances per thread.

## Future Enhancements

- HTTP/3 (QUIC) support
- Custom frame type registration
- Advanced HPACK table management
- Streaming body processing
- WebSocket upgrade support