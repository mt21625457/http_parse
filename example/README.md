# HTTP Parse Library Examples

This directory contains practical examples demonstrating how to use the HTTP Parse library.

## Examples Overview

### 1. Basic HTTP/1 Parsing (`basic_http1_parsing.cpp`)
Demonstrates basic HTTP/1.1 request parsing:
- Simple request parsing
- Header access
- Error handling

### 2. HTTP/2 Connection (`http2_connection.cpp`)
Shows HTTP/2 connection handling:
- Connection setup
- Event callbacks
- Stream management
- Preface processing

### 3. Version Detection (`version_detection.cpp`)
Illustrates automatic HTTP version detection:
- Auto-detection capabilities
- Manual version forcing
- Error handling for invalid data

### 4. Incremental Parsing (`incremental_parsing.cpp`)
Demonstrates parsing data in chunks:
- Partial data handling
- Progressive parsing
- Parser state management
- Real-world streaming scenarios

### 5. C++23 Features (`cpp23_features.cpp`)
Showcases modern C++23 features:
- String literal operators
- Concepts for type safety
- Enhanced header access
- Pattern matching
- Ranges integration
- Structured bindings

## Building Examples

### Prerequisites
- C++17 or later compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Make (for using the provided Makefile)

### Using Makefile

```bash
# Build all examples with C++17
make

# Build with specific C++ standard
make cpp17    # C++17 examples
make cpp20    # C++20 examples  
make cpp23    # C++23 examples (includes C++23 features)

# Run specific examples
make run-basic       # Basic HTTP/1 parsing
make run-http2       # HTTP/2 connection
make run-detection   # Version detection
make run-incremental # Incremental parsing
make run-cpp23       # C++23 features (requires C++23)

# Run all examples
make run-all

# Clean build files
make clean
```

### Manual Compilation

#### C++17/20 Examples
```bash
g++ -std=c++17 -I../include -o basic_example basic_http1_parsing.cpp
g++ -std=c++17 -I../include -o http2_example http2_connection.cpp
g++ -std=c++17 -I../include -o detection_example version_detection.cpp
g++ -std=c++17 -I../include -o incremental_example incremental_parsing.cpp
```

#### C++23 Example
```bash
g++ -std=c++23 -I../include -o cpp23_example cpp23_features.cpp
```

## Example Outputs

### Basic HTTP/1 Parsing
```
Successfully parsed HTTP/1.1 request:
Method: GET
URI: /index.html
Host: example.com
User-Agent: Mozilla/5.0
Headers count: 4
```

### HTTP/2 Connection
```
Processing HTTP/2 connection preface...
Successfully processed 24 bytes of preface
Active streams: 0
```

### Version Detection
```
Testing HTTP version auto-detection...

1. Testing HTTP/1.1 request:
   ✓ Successfully parsed as HTTP/1.1
   Detected version: HTTP/1.1
   Method: GET
   URI: /api/users
   Headers: 3

2. Testing HTTP/2 preface:
   ✓ Successfully processed HTTP/2 preface
   Detected version: HTTP/2.0
```

### Incremental Parsing
```
Processing HTTP request in chunks:

Chunk 1: "GET /api/users HTTP/1.1\r\n"
Accumulated data size: 26 bytes
◦ Need more data to continue parsing

Chunk 2: "Host: api.example.com\r\n"
Accumulated data size: 49 bytes
◦ Partial parse - need more data

...

✓ Request parsing complete!
```

## Integration Guide

### Including the Library
```cpp
#include "http_parse/http_parse.hpp"
using namespace co::http;
```

### Basic Usage Pattern
```cpp
// 1. Create parser
parser p(version::auto_detect);

// 2. Parse data
request req;
auto result = p.parse_request(data, req);

// 3. Check result
if (result) {
    // Use parsed request
    std::cout << req.method << " " << req.uri << std::endl;
} else {
    // Handle error
    std::cerr << "Parse error" << std::endl;
}
```

### Error Handling
```cpp
auto result = parse_request(data);
if (!result) {
    switch (result.error()) {
        case error_code::need_more_data:
            // Wait for more data
            break;
        case error_code::protocol_error:
            // Invalid HTTP data
            break;
        case error_code::unsupported_version:
            // Unsupported HTTP version
            break;
    }
}
```

## Performance Tips

1. **Reuse Parsers**: Reset and reuse parser instances instead of creating new ones
2. **Incremental Parsing**: Use incremental parsing for streaming data
3. **Zero-Copy**: The library uses spans and string_views to avoid copying data
4. **Header Access**: Use `get_header()` for case-insensitive header lookup

## Troubleshooting

### Common Issues

1. **Compilation Errors with C++23 Features**
   - Ensure your compiler supports C++23
   - Use C++17/20 examples if C++23 is not available

2. **Missing Headers**
   - Make sure the include path points to the library headers
   - Use `-I../include` when compiling from the example directory

3. **Linker Errors**
   - This is a header-only library, no linking required
   - If using external dependencies, ensure they are properly linked

### Debugging

Enable debug output by defining preprocessor macros:
```cpp
#define HTTP_PARSE_DEBUG
#include "http_parse/http_parse.hpp"
```

## Contributing

When adding new examples:
1. Follow the existing naming convention
2. Add documentation comments
3. Update this README
4. Add appropriate Makefile targets
5. Test with multiple compiler versions