#include "../include/http_parse.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <thread>
#include <chrono>
#include <map>
#include <queue>

using namespace co::http;

// 模拟网络数据分片传输
std::vector<std::vector<uint8_t>> split_binary_data(const std::vector<uint8_t>& data, size_t chunk_size = 64) {
    std::vector<std::vector<uint8_t>> chunks;
    for (size_t i = 0; i < data.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, data.size());
        chunks.emplace_back(data.begin() + i, data.begin() + end);
    }
    return chunks;
}

// 输出二进制数据的十六进制表示
std::string to_hex(const std::vector<uint8_t>& data, size_t max_bytes = 32) {
    std::string result;
    size_t limit = std::min(data.size(), max_bytes);
    for (size_t i = 0; i < limit; ++i) {
        char buf[4];
        sprintf(buf, "%02x ", data[i]);
        result += buf;
    }
    if (data.size() > max_bytes) {
        result += "...";
    }
    return result;
}

void demo_http2_client_basic() {
    std::cout << "\n=== HTTP/2 客户端基础示例 ===\n";
    
    // 创建HTTP/2客户端连接
    auto client = http2::client();
    
    // 设置事件处理器
    std::map<uint32_t, std::string> response_bodies;
    std::queue<std::string> server_messages;
    
    client.on_response([&](uint32_t stream_id, const response& resp, bool end_stream) {
        std::cout << "📥 收到响应 [Stream " << stream_id << "]:\n";
        std::cout << "   状态码: " << static_cast<int>(resp.status_code_) << "\n";
        std::cout << "   头部数量: " << resp.headers_.size() << "\n";
        for (const auto& h : resp.headers_) {
            std::cout << "   " << h.name << ": " << h.value << "\n";
        }
        std::cout << "   End Stream: " << (end_stream ? "是" : "否") << "\n";
    });
    
    client.on_data([&](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        std::cout << "📦 收到数据 [Stream " << stream_id << "]:\n";
        std::cout << "   数据长度: " << data.size() << " 字节\n";
        
        // 收集响应体
        std::string data_str(reinterpret_cast<const char*>(data.data()), data.size());
        response_bodies[stream_id] += data_str;
        
        if (end_stream) {
            std::cout << "   完整响应体: " << response_bodies[stream_id] << "\n";
        }
        std::cout << "   End Stream: " << (end_stream ? "是" : "否") << "\n";
    });
    
    client.on_stream_end([&](uint32_t stream_id) {
        std::cout << "🔚 流结束 [Stream " << stream_id << "]\n";
    });
    
    client.on_settings([&](const v2::connection_settings& settings) {
        std::cout << "⚙️  收到设置更新:\n";
        std::cout << "   Header Table Size: " << settings.header_table_size << "\n";
        std::cout << "   Enable Push: " << (settings.enable_push ? "是" : "否") << "\n";
        std::cout << "   Max Concurrent Streams: " << settings.max_concurrent_streams << "\n";
        std::cout << "   Initial Window Size: " << settings.initial_window_size << "\n";
    });
    
    client.on_ping([&](std::span<const uint8_t, 8> data, bool ack) {
        std::cout << "🏓 收到PING " << (ack ? "ACK" : "") << ": ";
        for (auto byte : data) {
            printf("%02x ", byte);
        }
        std::cout << "\n";
    });
    
    // 获取连接前置数据
    auto preface = client.get_preface();
    std::cout << "🚀 HTTP/2连接前置数据 (" << preface.size() << " 字节):\n";
    std::cout << "   " << std::string(preface) << "\n";
    
    // 1. 发送GET请求
    std::cout << "\n1. 发送GET请求:\n";
    auto get_req = http1::request()
        .method(method::get)
        .target("/api/users/123")
        .header("User-Agent", "HTTP2-Client/1.0")
        .header("Accept", "application/json")
        .header("Authorization", "Bearer token123");
    
    auto get_buffer = client.send_request(get_req);
    if (get_buffer) {
        std::vector<uint8_t> get_data(get_buffer->span().begin(), get_buffer->span().end());
        std::cout << "   ✓ GET请求编码成功 (" << get_data.size() << " 字节)\n";
        std::cout << "   十六进制: " << to_hex(get_data) << "\n";
    }
    
    // 2. 发送POST请求
    std::cout << "\n2. 发送POST请求:\n";
    std::string json_data = R"({"name": "张三", "email": "zhang@example.com", "department": "技术部"})";
    auto post_req = http1::request()
        .method(method::post)
        .target("/api/users")
        .header("Content-Type", "application/json; charset=utf-8")
        .header("User-Agent", "HTTP2-Client/1.0")
        .header("Accept", "application/json")
        .body(json_data);
    
    auto post_buffer = client.send_request(post_req);
    if (post_buffer) {
        std::vector<uint8_t> post_data(post_buffer->span().begin(), post_buffer->span().end());
        std::cout << "   ✓ POST请求编码成功 (" << post_data.size() << " 字节)\n";
        std::cout << "   十六进制: " << to_hex(post_data) << "\n";
    }
    
    // 3. 发送控制帧
    std::cout << "\n3. 发送控制帧:\n";
    
    // 发送PING
    std::array<uint8_t, 8> ping_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto ping_buffer = client.send_ping(ping_data);
    if (ping_buffer) {
        std::vector<uint8_t> ping_frame(ping_buffer->span().begin(), ping_buffer->span().end());
        std::cout << "   ✓ PING帧编码成功 (" << ping_frame.size() << " 字节)\n";
        std::cout << "   十六进制: " << to_hex(ping_frame) << "\n";
    }
    
    // 发送WINDOW_UPDATE
    auto window_buffer = client.send_window_update(0, 32768); // 连接级别窗口更新
    if (window_buffer) {
        std::vector<uint8_t> window_frame(window_buffer->span().begin(), window_buffer->span().end());
        std::cout << "   ✓ WINDOW_UPDATE帧编码成功 (" << window_frame.size() << " 字节)\n";
        std::cout << "   十六进制: " << to_hex(window_frame) << "\n";
    }
    
    // 4. 获取连接统计
    auto stats = client.get_stats();
    std::cout << "\n4. 连接统计:\n";
    std::cout << "   已处理帧数: " << stats.frames_processed << "\n";
    std::cout << "   数据帧数: " << stats.data_frames << "\n";
    std::cout << "   头部帧数: " << stats.headers_frames << "\n";
    std::cout << "   控制帧数: " << stats.control_frames << "\n";
    std::cout << "   错误数: " << stats.errors << "\n";
}

void demo_http2_server_basic() {
    std::cout << "\n=== HTTP/2 服务器基础示例 ===\n";
    
    // 创建HTTP/2服务器连接
    auto server = http2::server();
    
    // 存储接收到的请求
    std::map<uint32_t, request> pending_requests;
    std::map<uint32_t, std::string> request_bodies;
    
    server.on_request([&](uint32_t stream_id, const request& req, bool end_stream) {
        std::cout << "📨 收到请求 [Stream " << stream_id << "]:\n";
        std::cout << "   方法: " << static_cast<int>(req.method_) << "\n";
        std::cout << "   路径: " << req.target_ << "\n";
        std::cout << "   头部数量: " << req.headers_.size() << "\n";
        for (const auto& h : req.headers_) {
            std::cout << "   " << h.name << ": " << h.value << "\n";
        }
        
        pending_requests[stream_id] = req;
        
        if (end_stream && req.body_.empty()) {
            // 立即处理GET请求
            std::cout << "   🔄 处理GET请求...\n";
            
            // 构建响应
            std::string response_json;
            status_code status;
            
            if (req.target_ == "/api/users/123") {
                status = status_code::ok;
                response_json = R"({
    "id": 123,
    "name": "张三",
    "email": "zhang@example.com",
    "department": "技术部",
    "created_at": "2025-01-01T10:00:00Z"
})";
            } else if (req.target_.starts_with("/api/")) {
                status = status_code::not_found;
                response_json = R"({"error": "API端点未找到", "code": 404})";
            } else {
                status = status_code::bad_request;
                response_json = R"({"error": "无效请求", "code": 400})";
            }
            
            auto resp = http1::response()
                .status(status)
                .header("Content-Type", "application/json; charset=utf-8")
                .header("Server", "HTTP2-Server/1.0")
                .header("Date", "Wed, 01 Jan 2025 12:00:00 GMT")
                .header("Cache-Control", "no-cache")
                .body(response_json);
            
            auto resp_buffer = server.send_response(stream_id, resp);
            if (resp_buffer) {
                std::vector<uint8_t> resp_data(resp_buffer->span().begin(), resp_buffer->span().end());
                std::cout << "   ✅ 响应发送成功 (" << resp_data.size() << " 字节)\n";
                std::cout << "   十六进制: " << to_hex(resp_data) << "\n";
            }
        }
    });
    
    server.on_data([&](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        std::cout << "📦 收到数据 [Stream " << stream_id << "]:\n";
        std::cout << "   数据长度: " << data.size() << " 字节\n";
        
        // 收集请求体
        std::string data_str(reinterpret_cast<const char*>(data.data()), data.size());
        request_bodies[stream_id] += data_str;
        
        if (end_stream) {
            std::cout << "   完整请求体: " << request_bodies[stream_id] << "\n";
            
            // 处理POST请求
            if (pending_requests.find(stream_id) != pending_requests.end()) {
                auto& req = pending_requests[stream_id];
                req.body_ = request_bodies[stream_id];
                
                std::cout << "   🔄 处理POST请求...\n";
                
                // 模拟创建用户
                auto resp = http1::response()
                    .status(status_code::created)
                    .header("Content-Type", "application/json; charset=utf-8")
                    .header("Server", "HTTP2-Server/1.0")
                    .header("Location", "/api/users/124")
                    .body(R"({
    "status": "success",
    "message": "用户创建成功",
    "data": {
        "id": 124,
        "name": "张三",
        "email": "zhang@example.com",
        "department": "技术部",
        "created_at": "2025-01-01T12:30:00Z"
    }
})");
                
                auto resp_buffer = server.send_response(stream_id, resp);
                if (resp_buffer) {
                    std::vector<uint8_t> resp_data(resp_buffer->span().begin(), resp_buffer->span().end());
                    std::cout << "   ✅ POST响应发送成功 (" << resp_data.size() << " 字节)\n";
                }
                
                // 清理
                pending_requests.erase(stream_id);
                request_bodies.erase(stream_id);
            }
        }
    });
    
    server.on_stream_error([&](uint32_t stream_id, v2::h2_error_code error) {
        std::cout << "❌ 流错误 [Stream " << stream_id << "]: " << static_cast<int>(error) << "\n";
        
        // 清理错误流
        pending_requests.erase(stream_id);
        request_bodies.erase(stream_id);
    });
    
    server.on_connection_error([&](v2::h2_error_code error, std::string_view debug_info) {
        std::cout << "💥 连接错误: " << static_cast<int>(error);
        if (!debug_info.empty()) {
            std::cout << " (" << debug_info << ")";
        }
        std::cout << "\n";
    });
    
    std::cout << "🎯 HTTP/2服务器已准备就绪，等待请求...\n";
}

void demo_http2_frame_processing() {
    std::cout << "\n=== HTTP/2 帧处理示例 ===\n";
    
    auto client = http2::client();
    
    // 1. 生成各种类型的帧
    std::cout << "1. 生成HTTP/2帧:\n";
    
    // SETTINGS帧
    std::unordered_map<uint16_t, uint32_t> settings;
    settings[static_cast<uint16_t>(v2::settings_id::header_table_size)] = 8192;
    settings[static_cast<uint16_t>(v2::settings_id::enable_push)] = 0;
    settings[static_cast<uint16_t>(v2::settings_id::max_concurrent_streams)] = 128;
    settings[static_cast<uint16_t>(v2::settings_id::initial_window_size)] = 65536;
    
    auto settings_buffer = client.send_settings(settings);
    if (settings_buffer) {
        std::vector<uint8_t> settings_data(settings_buffer->span().begin(), settings_buffer->span().end());
        std::cout << "   ✓ SETTINGS帧 (" << settings_data.size() << " 字节): " << to_hex(settings_data) << "\n";
    }
    
    // HEADERS帧 (请求)
    auto req = http1::request()
        .method(method::get)
        .target("/api/stream/data")
        .header("Accept", "text/event-stream")
        .header("Cache-Control", "no-cache");
    
    auto headers_buffer = client.send_request(req, false); // 不结束流，准备发送数据
    if (headers_buffer) {
        std::vector<uint8_t> headers_data(headers_buffer->span().begin(), headers_buffer->span().end());
        std::cout << "   ✓ HEADERS帧 (" << headers_data.size() << " 字节): " << to_hex(headers_data) << "\n";
    }
    
    // DATA帧
    std::string stream_data = "data: {\"timestamp\": \"2025-01-01T12:00:00Z\", \"event\": \"user_login\"}\n\n";
    auto data_buffer = client.send_data(1, stream_data, false);
    if (data_buffer) {
        std::vector<uint8_t> data_frame(data_buffer->span().begin(), data_buffer->span().end());
        std::cout << "   ✓ DATA帧 (" << data_frame.size() << " 字节): " << to_hex(data_frame) << "\n";
    }
    
    // WINDOW_UPDATE帧
    auto window_buffer = client.send_window_update(1, 1024);
    if (window_buffer) {
        std::vector<uint8_t> window_data(window_buffer->span().begin(), window_buffer->span().end());
        std::cout << "   ✓ WINDOW_UPDATE帧 (" << window_data.size() << " 字节): " << to_hex(window_data) << "\n";
    }
    
    // RST_STREAM帧
    auto rst_buffer = client.send_rst_stream(3, v2::h2_error_code::cancel);
    if (rst_buffer) {
        std::vector<uint8_t> rst_data(rst_buffer->span().begin(), rst_buffer->span().end());
        std::cout << "   ✓ RST_STREAM帧 (" << rst_data.size() << " 字节): " << to_hex(rst_data) << "\n";
    }
    
    // GOAWAY帧
    auto goaway_buffer = client.send_goaway(v2::h2_error_code::no_error, "正常关闭连接");
    if (goaway_buffer) {
        std::vector<uint8_t> goaway_data(goaway_buffer->span().begin(), goaway_buffer->span().end());
        std::cout << "   ✓ GOAWAY帧 (" << goaway_data.size() << " 字节): " << to_hex(goaway_data) << "\n";
    }
}

void demo_http2_streaming_data() {
    std::cout << "\n=== HTTP/2 流式数据传输示例 ===\n";
    
    // 模拟客户端和服务器之间的流式数据传输
    auto client = http2::client();
    auto server = http2::server();
    
    // 客户端接收流式响应
    std::map<uint32_t, std::string> streaming_data;
    client.on_data([&](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        std::string chunk(reinterpret_cast<const char*>(data.data()), data.size());
        streaming_data[stream_id] += chunk;
        
        std::cout << "📡 [Stream " << stream_id << "] 接收数据块 (" << data.size() << " 字节)\n";
        std::cout << "   内容: " << chunk;
        
        if (end_stream) {
            std::cout << "📋 [Stream " << stream_id << "] 完整数据: " << streaming_data[stream_id] << "\n";
        }
    });
    
    // 1. 模拟发送大文件的多个DATA帧
    std::cout << "1. 模拟大文件传输:\n";
    
    std::string large_content = R"(
这是一个大型文件的内容，用来演示HTTP/2协议的流式数据传输。
HTTP/2支持多路复用，可以同时处理多个流。
每个流可以独立发送数据，不会阻塞其他流。

文件内容包含多行数据：
- 第1行：用户信息数据
- 第2行：产品目录数据  
- 第3行：订单历史数据
- 第4行：系统日志数据
- 第5行：统计报表数据

HTTP/2的帧结构允许高效的数据传输和流控制。
HPACK压缩算法可以减少头部开销。
服务器推送功能可以主动发送资源。
)";
    
    // 将大文件分块发送
    const size_t chunk_size = 100;
    for (size_t i = 0; i < large_content.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, large_content.size());
        std::string chunk = large_content.substr(i, end - i);
        bool is_last = (end >= large_content.size());
        
        auto data_buffer = client.send_data(5, chunk, is_last);
        if (data_buffer) {
            std::cout << "   📤 发送数据块 " << (i/chunk_size + 1) << " (" << chunk.size() << " 字节)";
            if (is_last) std::cout << " [最后一块]";
            std::cout << "\n";
            
            // 模拟客户端处理这个数据块
            std::vector<uint8_t> chunk_bytes(chunk.begin(), chunk.end());
            // 这里可以调用 client.process() 来模拟处理
        }
    }
    
    // 2. 模拟并发流
    std::cout << "\n2. 模拟并发流传输:\n";
    
    // 同时发送3个不同的请求到不同的流
    std::vector<std::pair<uint32_t, std::string>> concurrent_streams = {
        {7, "/api/users"},
        {9, "/api/products"}, 
        {11, "/api/orders"}
    };
    
    for (const auto& [stream_id, path] : concurrent_streams) {
        auto req = http1::request()
            .method(method::get)
            .target(path)
            .header("Accept", "application/json");
        
        // 这里演示原理，实际需要修改send_request支持指定stream_id
        std::cout << "   🌊 启动Stream " << stream_id << " -> " << path << "\n";
        
        // 模拟异步响应
        std::string response_data = "{\"stream_id\": " + std::to_string(stream_id) + 
                                  ", \"path\": \"" + path + "\", \"data\": \"...响应数据...\"}";
        
        auto data_buffer = server.send_data(stream_id, response_data, true);
        if (data_buffer) {
            std::cout << "   📤 Stream " << stream_id << " 响应发送 (" << response_data.size() << " 字节)\n";
        }
    }
}

void demo_http2_complete_communication() {
    std::cout << "\n=== HTTP/2 完整通信流程示例 ===\n";
    
    // 模拟完整的HTTP/2客户端-服务器通信
    std::cout << "模拟场景: RESTful API完整交互流程\n\n";
    
    auto client = http2::client();
    auto server = http2::server();
    
    // 存储通信数据
    std::vector<std::vector<uint8_t>> client_to_server_frames;
    std::vector<std::vector<uint8_t>> server_to_client_frames;
    
    // 1. 连接建立
    std::cout << "1. HTTP/2连接建立:\n";
    auto preface = client.get_preface();
    std::cout << "   📡 客户端发送连接前置: " << preface.size() << " 字节\n";
    
    // 模拟处理连接前置
    std::vector<uint8_t> preface_bytes(preface.begin(), preface.end());
    auto preface_result = server.process_connection_preface(preface_bytes);
    if (preface_result) {
        std::cout << "   ✅ 服务器处理前置成功\n";
    }
    
    // 2. 设置交换
    std::cout << "\n2. 设置帧交换:\n";
    std::unordered_map<uint16_t, uint32_t> client_settings;
    client_settings[static_cast<uint16_t>(v2::settings_id::header_table_size)] = 4096;
    client_settings[static_cast<uint16_t>(v2::settings_id::enable_push)] = 1;
    client_settings[static_cast<uint16_t>(v2::settings_id::max_concurrent_streams)] = 100;
    
    auto client_settings_buffer = client.send_settings(client_settings);
    if (client_settings_buffer) {
        std::vector<uint8_t> settings_data(client_settings_buffer->span().begin(), 
                                         client_settings_buffer->span().end());
        std::cout << "   📡 客户端发送设置 (" << settings_data.size() << " 字节)\n";
        client_to_server_frames.push_back(settings_data);
    }
    
    // 服务器回复设置ACK
    auto server_ack_buffer = server.send_settings_ack();
    if (server_ack_buffer) {
        std::vector<uint8_t> ack_data(server_ack_buffer->span().begin(), 
                                    server_ack_buffer->span().end());
        std::cout << "   📡 服务器发送设置ACK (" << ack_data.size() << " 字节)\n";
        server_to_client_frames.push_back(ack_data);
    }
    
    // 3. 多个并发请求
    std::cout << "\n3. 发送并发请求:\n";
    
    // 请求1: 获取用户列表
    auto users_req = http1::request()
        .method(method::get)
        .target("/api/users?page=1&limit=20")
        .header("Accept", "application/json")
        .header("Authorization", "Bearer jwt_token_here");
    
    auto users_buffer = client.send_request(users_req);
    if (users_buffer) {
        std::vector<uint8_t> users_data(users_buffer->span().begin(), users_buffer->span().end());
        std::cout << "   📤 Stream 1: GET /api/users (" << users_data.size() << " 字节)\n";
        client_to_server_frames.push_back(users_data);
    }
    
    // 请求2: 创建新用户  
    std::string user_json = R"({
    "name": "李四",
    "email": "lisi@example.com", 
    "department": "产品部",
    "role": "product_manager"
})";
    
    auto create_req = http1::request()
        .method(method::post)
        .target("/api/users")
        .header("Content-Type", "application/json")
        .header("Authorization", "Bearer jwt_token_here")
        .body(user_json);
    
    auto create_buffer = client.send_request(create_req);
    if (create_buffer) {
        std::vector<uint8_t> create_data(create_buffer->span().begin(), create_buffer->span().end());
        std::cout << "   📤 Stream 3: POST /api/users (" << create_data.size() << " 字节)\n";
        client_to_server_frames.push_back(create_data);
    }
    
    // 请求3: 上传文件
    std::string file_data = "这是一个测试文件的内容，用来演示文件上传功能...";
    auto upload_req = http1::request()
        .method(method::put)
        .target("/api/files/avatar.jpg")
        .header("Content-Type", "image/jpeg")
        .header("Content-Length", std::to_string(file_data.size()))
        .body(file_data);
    
    auto upload_buffer = client.send_request(upload_req);
    if (upload_buffer) {
        std::vector<uint8_t> upload_data(upload_buffer->span().begin(), upload_buffer->span().end());
        std::cout << "   📤 Stream 5: PUT /api/files/avatar.jpg (" << upload_data.size() << " 字节)\n";
        client_to_server_frames.push_back(upload_data);
    }
    
    // 4. 服务器响应
    std::cout << "\n4. 服务器生成响应:\n";
    
    // 响应1: 用户列表
    std::string users_response = R"({
    "status": "success",
    "data": [
        {"id": 1, "name": "张三", "email": "zhang@example.com"},
        {"id": 2, "name": "李四", "email": "lisi@example.com"}
    ],
    "pagination": {"page": 1, "limit": 20, "total": 2}
})";
    
    auto users_resp = http1::response()
        .status(status_code::ok)
        .header("Content-Type", "application/json")
        .header("Cache-Control", "max-age=300")
        .body(users_response);
    
    auto users_resp_buffer = server.send_response(1, users_resp);
    if (users_resp_buffer) {
        std::vector<uint8_t> resp_data(users_resp_buffer->span().begin(), users_resp_buffer->span().end());
        std::cout << "   📥 Stream 1 响应: 200 OK (" << resp_data.size() << " 字节)\n";
        server_to_client_frames.push_back(resp_data);
    }
    
    // 响应2: 创建用户成功
    std::string create_response = R"({
    "status": "success",
    "message": "用户创建成功",
    "data": {"id": 3, "name": "李四", "email": "lisi@example.com"}
})";
    
    auto create_resp = http1::response()
        .status(status_code::created)
        .header("Content-Type", "application/json")
        .header("Location", "/api/users/3")
        .body(create_response);
    
    auto create_resp_buffer = server.send_response(3, create_resp);
    if (create_resp_buffer) {
        std::vector<uint8_t> resp_data(create_resp_buffer->span().begin(), create_resp_buffer->span().end());
        std::cout << "   📥 Stream 3 响应: 201 Created (" << resp_data.size() << " 字节)\n";
        server_to_client_frames.push_back(resp_data);
    }
    
    // 响应3: 文件上传成功
    auto upload_resp = http1::response()
        .status(status_code::ok)
        .header("Content-Type", "application/json")
        .body(R"({"status": "success", "message": "文件上传成功", "url": "/files/avatar.jpg"})");
    
    auto upload_resp_buffer = server.send_response(5, upload_resp);
    if (upload_resp_buffer) {
        std::vector<uint8_t> resp_data(upload_resp_buffer->span().begin(), upload_resp_buffer->span().end());
        std::cout << "   📥 Stream 5 响应: 200 OK (" << resp_data.size() << " 字节)\n";
        server_to_client_frames.push_back(resp_data);
    }
    
    // 5. 连接统计
    std::cout << "\n5. 通信统计:\n";
    size_t total_client_bytes = 0;
    size_t total_server_bytes = 0;
    
    for (const auto& frame : client_to_server_frames) {
        total_client_bytes += frame.size();
    }
    for (const auto& frame : server_to_client_frames) {
        total_server_bytes += frame.size();
    }
    
    std::cout << "   📊 客户端发送: " << client_to_server_frames.size() << " 个帧, " 
              << total_client_bytes << " 字节\n";
    std::cout << "   📊 服务器发送: " << server_to_client_frames.size() << " 个帧, " 
              << total_server_bytes << " 字节\n";
    std::cout << "   📊 总数据量: " << (total_client_bytes + total_server_bytes) << " 字节\n";
    
    auto client_stats = client.get_stats();
    std::cout << "   📈 客户端统计: " << client_stats.frames_processed << " 帧已处理\n";
    
    std::cout << "\n✅ HTTP/2完整通信流程演示完成!\n";
}

int main() {
    std::cout << "HTTP/2 协议完整示例\n";
    std::cout << "========================\n";
    
    try {
        demo_http2_client_basic();
        demo_http2_server_basic();
        demo_http2_frame_processing();
        demo_http2_streaming_data();
        demo_http2_complete_communication();
        
        std::cout << "\n🎉 所有HTTP/2示例运行完成!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}