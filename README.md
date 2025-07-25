# HTTP Parse Library - ńC++23 HTTP/1.x & HTTP/2㐓

 *�4����'�HTTPO�㐓/HTTP/1.x�HTTP/2(��C++23y'

## y'

- **�4���** - � ���+s�(
- **HTTP/1.x�t/** - RFC7230/7231�|�
- **HTTP/2�t/** - RFC7540/7541/9113�|�+HPACK�)
- **ńAPI��** - ��HTTP/1�HTTP/2���(
- **�'�** - ���MŁ��XM
- **A** - /��㐌A
- **��C++23** - (std::expectedstd::spanI��y'

## � �

### HTTP/1.x �U(�

```cpp
#include "http_parse.hpp"
using namespace co::http;

// �HTTP�B
auto result = http1::parse_request("GET /api/users HTTP/1.1\r\nHost: example.com\r\n\r\n");
if (result) {
    auto& req = *result;
    std::cout << "Method: " << req.method_ << std::endl;
    std::cout << "Target: " << req.target_ << std::endl;
}

// ��HTTP͔
auto resp = http1::response()
    .status(status_code::ok)
    .header("Content-Type", "application/json")
    .body(R"({"message": "Hello World"})");

auto encoded = http1::encode_response(resp);
if (encoded) {
    std::cout << *encoded << std::endl;
}
```

### HTTP/1.x A�

```cpp
// ���'��B
http1::request_parser parser;
request req;

// �60�pnW
auto result1 = parser.parse("GET /api", req);
auto result2 = parser.parse("/users HTTP/1.1\r\n", req);
auto result3 = parser.parse("Host: example.com\r\n\r\n", req);

if (parser.is_complete()) {
    std::cout << "�B㐌!" << std::endl;
}
```

### HTTP/2 �7�(�

```cpp
#include "http_parse.hpp"
using namespace co::http;

// �HTTP/2�7�ޥ
auto client = http2::client()
    .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
        std::cout << "60͔ stream=" << stream_id 
                  << " status=" << static_cast<int>(resp.status_code_) << std::endl;
    })
    .on_data([](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        std::cout << "60pn stream=" << stream_id 
                  << " size=" << data.size() << std::endl;
    })
    .on_stream_end([](uint32_t stream_id) {
        std::cout << "A�_ stream=" << stream_id << std::endl;
    });

// ��B
auto req = http1::request()
    .method(method::get)
    .target("/api/users")
    .header("User-Agent", "HTTP2-Client/1.0");

auto send_buffer = client.send_request(req);
if (send_buffer) {
    // � send_buffer->data() 0Q�
    // �֢7�npn
    auto preface = client.get_preface();
    // �H��npn6��Bpn
}

// �h͔
uint8_t received_data[4096];
// �Qܥ6pn0 received_data
auto process_result = client.process(std::span<const uint8_t>(received_data, bytes_received));
```

### HTTP/2 �h(�

```cpp
#include "http_parse.hpp"
using namespace co::http;

// �HTTP/2�hޥ
auto server = http2::server()
    .on_request([&](uint32_t stream_id, const request& req, bool end_stream) {
        std::cout << "60�B stream=" << stream_id 
                  << " method=" << static_cast<int>(req.method_)
                  << " path=" << req.target_ << std::endl;
        
        // ��͔
        auto resp = http1::response()
            .status(status_code::ok)
            .header("Content-Type", "application/json")
            .body(R"({"result": "success"})");
        
        // �͔
        auto send_buffer = server.send_response(stream_id, resp);
        if (send_buffer) {
            // �͔pn0Q�
        }
    })
    .on_stream_error([](uint32_t stream_id, v2::h2_error_code error) {
        std::cout << "A� stream=" << stream_id 
                  << " error=" << static_cast<int>(error) << std::endl;
    });

// �7�pn
uint8_t received_data[4096];
auto process_result = server.process(std::span<const uint8_t>(received_data, bytes_received));
```

### HTTP/2 اy'

```cpp
// A�6
auto window_update = client.send_window_update(stream_id, 65536);

// �nMn
std::unordered_map<uint16_t, uint32_t> settings;
settings[static_cast<uint16_t>(v2::settings_id::max_concurrent_streams)] = 100;
settings[static_cast<uint16_t>(v2::settings_id::initial_window_size)] = 65536;
auto settings_frame = client.send_settings(settings);

// �PING
std::array<uint8_t, 8> ping_data = {1, 2, 3, 4, 5, 6, 7, 8};
auto ping_frame = client.send_ping(ping_data);

// ��ޥߡ
auto stats = client.get_stats();
std::cout << "�'p: " << stats.frames_processed << std::endl;
std::cout << "pn'p: " << stats.data_frames << std::endl;
std::cout << "4�'p: " << stats.headers_frames << std::endl;
```

## '�y'

### ����

```cpp
// (�'��:Mstd::vector<uint8_t>
output_buffer buffer;
auto result = http1::encode_request(req, buffer);

// ������
std::string_view data = buffer.view();
std::span<const uint8_t> bytes = buffer.span();
```

### AAPI

```cpp
// HTTP/1.x A�h
http1::request_parser parser;
request req;

while (has_more_data()) {
    auto chunk = get_next_chunk();
    auto result = parser.parse(chunk, req);
    
    if (parser.is_complete()) {
        process_request(req);
        parser.reset(); // : *�B�n
        break;
    }
}
```

## API�

### HTTP/1.x}z� (`http1`)

#### �U�p
- `parse_request(data)` - 㐌t�B
- `parse_response(data)` - 㐌t͔  
- `encode_request(req)` - �B:W&2
- `encode_response(resp)` - ͔:W&2
- `encode_request(req, buffer)` - 0�'��:
- `encode_response(resp, buffer)` - 0�'��:

#### A�h
- `stream_parser<request>` (+: `request_parser`)
- `stream_parser<response>` (+: `response_parser`)

#### ��h
- `request()` - ���B��h
- `response()` - ��͔��h

### HTTP/2}z� (`http2`)

#### ޥ{
- `connection(is_server)` - HTTP/2ޥ�h
- `client()` - ��7�ޥ
- `server()` - ��hޥ

#### ;���
- `process(data)` - �6pn
- `send_request(req)` - ��B(�7�)
- `send_response(stream_id, resp)` - �͔(�h)
- `send_data(stream_id, data)` - �pn'
- `send_settings(settings)` - ��n
- `send_ping(data)` - �PING
- `send_goaway(error)` - �GOAWAY

#### ��h
- `on_request(handler)` - �Bh
- `on_response(handler)` - ͔h
- `on_data(handler)` - pnh
- `on_stream_end(handler)` - A�_h
- `on_stream_error(handler)` - A�h
- `on_connection_error(handler)` - ޥ�h

### � �� (`http_parse`)

Л 8(����w�

```cpp
// HTTP/1�w�
http_parse::parse_request(data);
http_parse::parse_response(data);
http_parse::encode_request(req);
http_parse::encode_response(resp);

// ��h�w�
http_parse::request();
http_parse::response();

// HTTP/2�w�
http_parse::http2_client();
http_parse::http2_server();
```

## сB

- C++23|��h (GCC 13+, Clang 16+, MSVC 19.37+)
- 4���� ���U�

## +�

```cpp
#include "http_parse.hpp"
using namespace co::http;
```

�*�Л� ��H(�HTTPO����'��B��(:o