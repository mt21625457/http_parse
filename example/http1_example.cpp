#include "../include/http_parse.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

using namespace co::http;

// 模拟网络数据分片传输
std::vector<std::string> split_data(const std::string& data, size_t chunk_size = 32) {
    std::vector<std::string> chunks;
    for (size_t i = 0; i < data.size(); i += chunk_size) {
        chunks.push_back(data.substr(i, std::min(chunk_size, data.size() - i)));
    }
    return chunks;
}

void demo_http1_simple_parsing() {
    std::cout << "\n=== HTTP/1.x 简单解析示例 ===\n";
    
    // 1. 解析GET请求
    std::string get_request = 
        "GET /api/users?page=1&limit=10 HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "User-Agent: HttpClient/1.0\r\n"
        "Accept: application/json\r\n"
        "Authorization: Bearer token123\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    auto req_result = http1::parse_request(get_request);
    if (req_result) {
        const auto& req = *req_result;
        std::cout << "✓ 解析GET请求成功:\n";
        std::cout << "  Method: " << static_cast<int>(req.method_) << "\n";
        std::cout << "  Target: " << req.target_ << "\n";
        std::cout << "  Version: " << static_cast<int>(req.version_) << "\n";
        std::cout << "  Headers (" << req.headers_.size() << "):\n";
        for (const auto& h : req.headers_) {
            std::cout << "    " << h.name << ": " << h.value << "\n";
        }
    } else {
        std::cout << "✗ 解析GET请求失败\n";
    }
    
    // 2. 解析POST请求带body
    std::string post_request = 
        "POST /api/users HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 45\r\n"
        "User-Agent: HttpClient/1.0\r\n"
        "\r\n"
        R"({"name": "张三", "email": "zhang@example.com"})";
    
    auto post_result = http1::parse_request(post_request);
    if (post_result) {
        const auto& req = *post_result;
        std::cout << "\n✓ 解析POST请求成功:\n";
        std::cout << "  Method: " << static_cast<int>(req.method_) << "\n";
        std::cout << "  Target: " << req.target_ << "\n";
        std::cout << "  Body: " << req.body_ << "\n";
    }
    
    // 3. 解析HTTP响应
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: 85\r\n"
        "Server: nginx/1.20.1\r\n"
        "Date: Wed, 01 Jan 2025 12:00:00 GMT\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n"
        R"({"status": "success", "data": {"id": 123, "name": "张三", "created": "2025-01-01"}})";
    
    auto resp_result = http1::parse_response(response_data);
    if (resp_result) {
        const auto& resp = *resp_result;
        std::cout << "\n✓ 解析HTTP响应成功:\n";
        std::cout << "  Status: " << static_cast<int>(resp.status_code_) << "\n";
        std::cout << "  Reason: " << resp.reason_phrase_ << "\n";
        std::cout << "  Body: " << resp.body_ << "\n";
    }
}

void demo_http1_streaming_parsing() {
    std::cout << "\n=== HTTP/1.x 流式解析示例 ===\n";
    
    // 模拟大型请求数据分片接收
    std::string large_request = 
        "POST /api/upload HTTP/1.1\r\n"
        "Host: upload.example.com\r\n"
        "Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
        "Content-Length: 512\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "\r\n"
        "------WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "这是一个测试文件的内容，用来演示HTTP/1.x协议的流式解析功能。\r\n"
        "支持大文件上传和分片处理，能够高效处理各种类型的HTTP请求。\r\n"
        "文件内容可以很长，通过流式解析可以减少内存使用。\r\n"
        "------WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
        "Content-Disposition: form-data; name=\"description\"\r\n"
        "\r\n"
        "文件描述信息\r\n"
        "------WebKitFormBoundary7MA4YWxkTrZu0gW--\r\n";
    
    // 将数据分成小块模拟网络传输
    auto chunks = split_data(large_request, 64);
    
    std::cout << "模拟接收 " << chunks.size() << " 个数据块:\n";
    
    http1::request_parser parser;
    request req;
    size_t total_parsed = 0;
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        std::cout << "  块 " << (i+1) << " (" << chunk.size() << " 字节): ";
        
        auto result = parser.parse(chunk, req);
        if (result) {
            total_parsed += *result;
            std::cout << "解析了 " << *result << " 字节";
            
            if (parser.is_complete()) {
                std::cout << " [完成]";
                break;
            } else if (parser.needs_more_data()) {
                std::cout << " [需要更多数据]";
            }
        } else {
            std::cout << "解析错误";
        }
        std::cout << "\n";
    }
    
    if (parser.is_complete()) {
        std::cout << "\n✓ 流式解析完成!\n";
        std::cout << "  总共解析: " << total_parsed << " 字节\n";
        std::cout << "  Method: " << static_cast<int>(req.method_) << "\n";
        std::cout << "  Target: " << req.target_ << "\n";
        std::cout << "  Body 长度: " << req.body_.size() << " 字节\n";
    }
}

void demo_http1_encoding() {
    std::cout << "\n=== HTTP/1.x 编码示例 ===\n";
    
    // 1. 构建并编码GET请求
    auto get_req = http1::request()
        .method(method::get)
        .target("/api/products?category=electronics&sort=price")
        .version(version::http_1_1)
        .header("Host", "shop.example.com")
        .header("User-Agent", "HttpClient/2.0")
        .header("Accept", "application/json")
        .header("Accept-Encoding", "gzip, deflate")
        .header("Connection", "keep-alive");
    
    auto get_encoded = http1::encode_request(get_req);
    if (get_encoded) {
        std::cout << "✓ GET请求编码:\n";
        std::cout << *get_encoded << "\n";
    }
    
    // 2. 构建并编码POST请求
    std::string json_payload = R"({
    "product": {
        "name": "智能手机",
        "price": 2999.99,
        "category": "electronics",
        "specs": {
            "memory": "128GB",
            "color": "黑色"
        }
    }
})";
    
    auto post_req = http1::request()
        .method(method::post)
        .target("/api/products")
        .version(version::http_1_1)
        .header("Host", "shop.example.com")
        .header("Content-Type", "application/json; charset=utf-8")
        .header("Content-Length", std::to_string(json_payload.size()))
        .header("User-Agent", "HttpClient/2.0")
        .header("Authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9")
        .body(json_payload);
    
    auto post_encoded = http1::encode_request(post_req);
    if (post_encoded) {
        std::cout << "✓ POST请求编码:\n";
        std::cout << *post_encoded << "\n";
    }
    
    // 3. 构建并编码HTTP响应
    std::string response_json = R"({
    "status": "success",
    "message": "产品创建成功",
    "data": {
        "id": 12345,
        "name": "智能手机",
        "created_at": "2025-01-01T12:00:00Z"
    }
})";
    
    auto response = http1::response()
        .status(status_code::created)
        .reason("Created")
        .version(version::http_1_1)
        .header("Content-Type", "application/json; charset=utf-8")
        .header("Content-Length", std::to_string(response_json.size()))
        .header("Server", "ApiServer/1.0")
        .header("Date", "Wed, 01 Jan 2025 12:00:00 GMT")
        .header("Location", "/api/products/12345")
        .header("Cache-Control", "no-cache")
        .body(response_json);
    
    auto resp_encoded = http1::encode_response(response);
    if (resp_encoded) {
        std::cout << "✓ HTTP响应编码:\n";
        std::cout << *resp_encoded << "\n";
    }
}

void demo_http1_high_performance() {
    std::cout << "\n=== HTTP/1.x 高性能缓冲区示例 ===\n";
    
    // 使用高性能缓冲区避免字符串拷贝
    auto req = http1::request()
        .method(method::put)
        .target("/api/users/123")
        .header("Host", "api.example.com")
        .header("Content-Type", "application/json")
        .body(R"({"name": "李四", "email": "lisi@example.com", "age": 28})");
    
    output_buffer buffer;
    auto result = http1::encode_request(req, buffer);
    
    if (result) {
        std::cout << "✓ 高性能编码成功，写入 " << *result << " 字节\n";
        
        // 零拷贝访问
        auto view = buffer.view();
        auto span = buffer.span();
        
        std::cout << "✓ 缓冲区大小: " << buffer.size() << " 字节\n";
        std::cout << "✓ string_view 长度: " << view.size() << "\n";
        std::cout << "✓ span 长度: " << span.size() << "\n";
        
        std::cout << "编码结果预览 (前200字符):\n";
        std::cout << view.substr(0, 200) << "...\n";
    }
}

void demo_http1_error_handling() {
    std::cout << "\n=== HTTP/1.x 错误处理示例 ===\n";
    
    // 测试各种错误情况
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"无效方法", "INVALID /path HTTP/1.1\r\n\r\n"},
        {"无效版本", "GET /path HTTP/2.5\r\n\r\n"},
        {"缺少Host", "GET /path HTTP/1.1\r\n\r\n"},
        {"无效头部", "GET /path HTTP/1.1\r\nInvalid-Header\r\n\r\n"},
        {"不完整数据", "GET /path HTTP/1.1\r\nHost: example.com\r\n"},
    };
    
    for (const auto& [desc, data] : test_cases) {
        std::cout << "测试: " << desc << "\n";
        auto result = http1::parse_request(data);
        if (!result) {
            std::cout << "  ✓ 正确检测到错误\n";
        } else {
            std::cout << "  ✗ 应该检测到错误但没有\n";
        }
    }
}

void demo_http1_complete_communication() {
    std::cout << "\n=== HTTP/1.x 完整通信流程示例 ===\n";
    
    // 模拟客户端-服务器完整交互
    std::cout << "模拟场景: 用户登录API调用\n\n";
    
    // 1. 客户端构建登录请求
    std::cout << "1. 客户端构建登录请求:\n";
    auto login_req = http1::request()
        .method(method::post)
        .target("/api/auth/login")
        .header("Host", "auth.example.com")
        .header("Content-Type", "application/json")
        .header("User-Agent", "MobileApp/1.5.0")
        .header("Accept", "application/json")
        .body(R"({"username": "user123", "password": "secret456"})");
    
    auto req_data = http1::encode_request(login_req);
    std::cout << "   编码的请求数据 (" << req_data->size() << " 字节)\n";
    
    // 2. 模拟网络传输 - 分片发送
    std::cout << "\n2. 模拟网络传输 (分片):\n";
    auto chunks = split_data(*req_data, 50);
    std::cout << "   分成 " << chunks.size() << " 个数据包传输\n";
    
    // 3. 服务器端流式接收和解析
    std::cout << "\n3. 服务器端流式解析:\n";
    http1::request_parser server_parser;
    request received_req;
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        auto parse_result = server_parser.parse(chunks[i], received_req);
        std::cout << "   接收数据包 " << (i+1) << " (" << chunks[i].size() << " 字节)\n";
        
        if (server_parser.is_complete()) {
            std::cout << "   ✓ 请求解析完成!\n";
            break;
        }
    }
    
    // 4. 服务器处理请求并构建响应
    std::cout << "\n4. 服务器处理并构建响应:\n";
    std::cout << "   解析到的请求: " << received_req.target_ << "\n";
    std::cout << "   请求体: " << received_req.body_ << "\n";
    
    // 模拟验证成功
    auto login_resp = http1::response()
        .status(status_code::ok)
        .header("Content-Type", "application/json")
        .header("Server", "AuthServer/2.1")
        .header("Set-Cookie", "session_id=abc123; HttpOnly; Secure")
        .header("Cache-Control", "no-store")
        .body(R"({
    "status": "success",
    "message": "登录成功",
    "data": {
        "user_id": 12345,
        "username": "user123",
        "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
        "expires_in": 3600
    }
})");
    
    auto resp_data = http1::encode_response(login_resp);
    std::cout << "   响应编码完成 (" << resp_data->size() << " 字节)\n";
    
    // 5. 客户端接收响应
    std::cout << "\n5. 客户端解析响应:\n";
    auto client_resp = http1::parse_response(*resp_data);
    if (client_resp) {
        std::cout << "   ✓ 响应解析成功!\n";
        std::cout << "   状态码: " << static_cast<int>(client_resp->status_code_) << "\n";
        std::cout << "   响应体长度: " << client_resp->body_.size() << " 字节\n";
        
        // 查找Set-Cookie头
        for (const auto& h : client_resp->headers_) {
            if (h.name == "set-cookie") {
                std::cout << "   设置Cookie: " << h.value << "\n";
            }
        }
    }
    
    std::cout << "\n✓ 完整的HTTP/1.x通信流程演示完成!\n";
}

int main() {
    std::cout << "HTTP/1.x 协议完整示例\n";
    std::cout << "========================\n";
    
    try {
        demo_http1_simple_parsing();
        demo_http1_streaming_parsing();
        demo_http1_encoding();
        demo_http1_high_performance();
        demo_http1_error_handling();
        demo_http1_complete_communication();
        
        std::cout << "\n🎉 所有HTTP/1.x示例运行完成!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}