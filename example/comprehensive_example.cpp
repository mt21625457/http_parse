#include "../include/http_parse.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <thread>
#include <chrono>
#include <random>
#include <queue>
#include <map>

using namespace co::http;

// 网络模拟器 - 模拟真实的网络传输条件
class NetworkSimulator {
public:
    struct Packet {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point arrival_time;
        bool is_lost = false;
    };
    
    NetworkSimulator(double loss_rate = 0.01, int min_delay_ms = 5, int max_delay_ms = 50) 
        : loss_rate_(loss_rate), min_delay_(min_delay_ms), max_delay_(max_delay_ms), 
          rng_(std::random_device{}()) {}
    
    // 发送数据包 (模拟网络传输)
    void send_packet(const std::vector<uint8_t>& data, const std::string& direction) {
        Packet packet;
        packet.data = data;
        
        // 模拟网络延迟
        auto delay = std::uniform_int_distribution<int>(min_delay_, max_delay_)(rng_);
        packet.arrival_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
        
        // 模拟丢包
        if (std::uniform_real_distribution<double>(0.0, 1.0)(rng_) < loss_rate_) {
            packet.is_lost = true;
            std::cout << "📦❌ [" << direction << "] 数据包丢失 (" << data.size() << " 字节)\n";
        } else {
            std::cout << "📦📡 [" << direction << "] 发送数据包 (" << data.size() << " 字节, 延迟 " << delay << "ms)\n";
        }
        
        packet_queue_.push(packet);
    }
    
    // 接收可用的数据包
    std::vector<std::vector<uint8_t>> receive_available_packets() {
        std::vector<std::vector<uint8_t>> available;
        auto now = std::chrono::steady_clock::now();
        
        while (!packet_queue_.empty()) {
            auto& packet = packet_queue_.front();
            
            if (packet.arrival_time <= now && !packet.is_lost) {
                available.push_back(std::move(packet.data));
                std::cout << "📦✅ 接收数据包 (" << available.back().size() << " 字节)\n";
            } else if (packet.arrival_time > now) {
                break; // 还没到达
            }
            
            packet_queue_.pop();
        }
        
        return available;
    }
    
    bool has_pending_packets() const {
        return !packet_queue_.empty();
    }
    
private:
    double loss_rate_;
    int min_delay_;
    int max_delay_;
    std::mt19937 rng_;
    std::queue<Packet> packet_queue_;
};

// HTTP/1.x 多数据块传输示例
void demo_http1_chunked_transfer() {
    std::cout << "\n=== HTTP/1.x 多数据块传输演示 ===\n";
    
    NetworkSimulator network(0.05, 10, 100); // 5%丢包率, 10-100ms延迟
    
    // 1. 模拟客户端发送大型POST请求
    std::cout << "1. 客户端构建大型POST请求:\n";
    
    // 生成大型JSON数据
    std::string large_json = R"({
    "operation": "bulk_user_import",
    "metadata": {
        "timestamp": "2025-01-01T12:00:00Z",
        "source": "csv_upload",
        "total_records": 1000
    },
    "users": [)";
    
    // 添加1000个用户记录
    for (int i = 1; i <= 1000; ++i) {
        large_json += "\n        {";
        large_json += "\"id\": " + std::to_string(i) + ", ";
        large_json += "\"name\": \"用户" + std::to_string(i) + "\", ";
        large_json += "\"email\": \"user" + std::to_string(i) + "@example.com\", ";
        large_json += "\"department\": \"部门" + std::to_string((i % 10) + 1) + "\", ";
        large_json += "\"created_at\": \"2025-01-01T12:00:00Z\"";
        large_json += "}";
        if (i < 1000) large_json += ",";
    }
    large_json += "\n    ]\n}";
    
    auto bulk_request = http1::request()
        .method(method::post)
        .uri("/api/users/bulk-import")
        .header("Host", "api.example.com")
        .header("Content-Type", "application/json; charset=utf-8")
        .header("Content-Length", std::to_string(large_json.size()))
        .header("User-Agent", "BulkImporter/1.0")
        .header("Authorization", "Bearer bulk_import_token")
        .body(large_json);
    
    auto encoded_request = http1::encode_request(bulk_request);
    if (!encoded_request) {
        std::cout << "   ✗ 请求编码失败\n";
        return;
    }
    std::cout << "   ✓ 大型请求构建完成 (" << encoded_request->size() << " 字节)\n";
    
    // 2. 将请求分块传输
    std::cout << "\n2. 分块传输请求:\n";
    const size_t chunk_size = 512; // 每块512字节
    std::vector<std::string> request_chunks;
    
    for (size_t i = 0; i < encoded_request->size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, encoded_request->size());
        request_chunks.push_back(encoded_request->substr(i, end - i));
    }
    
    std::cout << "   📦 请求分成 " << request_chunks.size() << " 个数据块\n";
    
    // 模拟网络传输
    for (size_t i = 0; i < request_chunks.size(); ++i) {
        std::vector<uint8_t> chunk_data(request_chunks[i].begin(), request_chunks[i].end());
        network.send_packet(chunk_data, "客户端->服务器");
    }
    
    // 3. 服务器端流式接收
    std::cout << "\n3. 服务器流式接收:\n";
    
    http1::request_parser server_parser;
    request received_request;
    size_t total_received = 0;
    
    // 模拟时间流逝，处理网络数据包
    auto start_time = std::chrono::steady_clock::now();
    while (network.has_pending_packets() || !server_parser.is_complete()) {
        // 模拟等待网络数据包
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        auto packets = network.receive_available_packets();
        for (const auto& packet_data : packets) {
            std::string chunk(packet_data.begin(), packet_data.end());
            auto parse_result = server_parser.parse(std::string_view(chunk), received_request);
            
            if (parse_result) {
                total_received += *parse_result;
                std::cout << "   📥 解析 " << *parse_result << " 字节 (总计: " << total_received << " 字节)\n";
                
                if (server_parser.is_complete()) {
                    auto end_time = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    
                    std::cout << "   ✅ 请求接收完成! 耗时: " << duration.count() << "ms\n";
                    std::cout << "   📊 接收统计:\n";
                    std::cout << "      - 总字节数: " << total_received << "\n";
                    std::cout << "      - 请求方法: " << static_cast<int>(received_request.method_type) << "\n";
                    std::cout << "      - 请求路径: " << received_request.target << "\n";
                    std::cout << "      - 请求体大小: " << received_request.body.size() << " 字节\n";
                    break;
                }
            } else {
                std::cout << "   ❌ 解析错误\n";
                break;
            }
        }
        
        // 超时检查
        if (std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count() > 10) {
            std::cout << "   ⏰ 接收超时\n";
            break;
        }
    }
    
    // 4. 服务器处理并响应
    std::cout << "\n4. 服务器处理并生成响应:\n";
    
    // 模拟处理时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto processing_response = http1::response()
        .status(202)
        .header("Content-Type", "application/json")
        .header("Server", "BulkProcessor/1.0")
        .header("X-Request-ID", "req_12345")
        .header("X-Processing-Time", "100ms")
        .body(R"({
    "status": "accepted", 
    "message": "批量导入请求已接收，正在处理中",
    "request_id": "req_12345",
    "estimated_time": "30 seconds",
    "imported_count": 0,
    "total_count": 1000
})");
    
    auto response_data = http1::encode_response(processing_response);
    if (!response_data) {
        std::cout << "   ✗ 响应编码失败\n";
        return;
    }
    std::cout << "   ✓ 响应生成完成 (" << response_data->size() << " 字节)\n";
    
    // 5. 响应分块返回
    std::cout << "\n5. 分块返回响应:\n";
    
    std::vector<std::string> response_chunks;
    for (size_t i = 0; i < response_data->size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, response_data->size());
        response_chunks.push_back(response_data->substr(i, end - i));
    }
    
    for (size_t i = 0; i < response_chunks.size(); ++i) {
        std::vector<uint8_t> chunk_data(response_chunks[i].begin(), response_chunks[i].end());
        network.send_packet(chunk_data, "服务器->客户端");
    }
    
    // 6. 客户端接收响应
    std::cout << "\n6. 客户端接收响应:\n";
    
    http1::response_parser client_parser;
    response received_response;
    
    start_time = std::chrono::steady_clock::now();
    while (network.has_pending_packets() || !client_parser.is_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        auto packets = network.receive_available_packets();
        for (const auto& packet_data : packets) {
            std::string chunk(packet_data.begin(), packet_data.end());
            auto parse_result = client_parser.parse(std::string_view(chunk), received_response);
            
            if (parse_result) {
                std::cout << "   📥 客户端解析 " << *parse_result << " 字节\n";
                
                if (client_parser.is_complete()) {
                    std::cout << "   ✅ 响应接收完成!\n";
                    std::cout << "   📊 响应状态: " << static_cast<int>(received_response.status_code) << "\n";
                    std::cout << "   📊 响应体: " << received_response.body << "\n";
                    break;
                }
            }
        }
        
        if (std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count() > 10) {
            break;
        }
    }
}

// HTTP/2 多流并发传输示例
void demo_http2_concurrent_streams() {
    std::cout << "\n=== HTTP/2 多流并发传输演示 ===\n";
    
    NetworkSimulator network(0.02, 5, 30); // 2%丢包率, 5-30ms延迟
    
    auto client = http2::client();
    auto server = http2::server();
    
    // 存储多流数据
    std::map<uint32_t, std::string> stream_responses;
    std::map<uint32_t, std::chrono::steady_clock::time_point> stream_start_times;
    
    // 1. 设置客户端回调
    client.on_response([&](uint32_t stream_id, const response& resp, bool end_stream) {
        auto now = std::chrono::steady_clock::now();
        if (stream_start_times.find(stream_id) != stream_start_times.end()) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - stream_start_times[stream_id]);
            std::cout << "📥 [Stream " << stream_id << "] 响应头 - 状态: " 
                      << static_cast<int>(resp.status_code) 
                      << " (耗时: " << duration.count() << "ms)\n";
        }
    });
    
    client.on_data([&](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        std::string data_str(reinterpret_cast<const char*>(data.data()), data.size());
        stream_responses[stream_id] += data_str;
        
        std::cout << "📦 [Stream " << stream_id << "] 数据 (" << data.size() << " 字节)";
        if (end_stream) {
            auto now = std::chrono::steady_clock::now();
            if (stream_start_times.find(stream_id) != stream_start_times.end()) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - stream_start_times[stream_id]);
                std::cout << " [完成 - 总耗时: " << duration.count() << "ms]";
            }
        }
        std::cout << "\n";
    });
    
    // 2. 设置服务器回调 
    std::map<uint32_t, request> server_requests;
    std::map<uint32_t, std::string> server_request_bodies;
    
    server.on_request([&](uint32_t stream_id, const request& req, bool end_stream) {
        std::cout << "📨 [Stream " << stream_id << "] 服务器收到请求: " 
                  << static_cast<int>(req.method_type) << " " << req.target << "\n";
        server_requests[stream_id] = req;
        
        // 对于GET请求，立即响应
        if (req.method_type == method::get && end_stream) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 + (stream_id % 50))); // 模拟处理时间
            
            std::string response_body;
            unsigned int status = 200;
            
            if (req.target == "/api/users") {
                response_body = R"({"users": [{"id": 1, "name": "张三"}, {"id": 2, "name": "李四"}]})";
            } else if (req.target == "/api/products") {
                response_body = R"({"products": [{"id": 101, "name": "笔记本电脑"}, {"id": 102, "name": "智能手机"}]})";
            } else if (req.target == "/api/orders") {
                response_body = R"({"orders": [{"id": 1001, "user_id": 1, "total": 2999.99}]})";
            } else if (req.target.starts_with("/api/slow")) {
                // 模拟慢请求
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                response_body = R"({"message": "慢请求处理完成", "processing_time": "200ms"})";
            } else {
                status = 404;
                response_body = R"({"error": "资源未找到"})";
            }
            
            auto resp = http1::response()
                .status(status)
                .header("Content-Type", "application/json")
                .header("Server", "HTTP2-Demo/1.0")
                .header("X-Stream-ID", std::to_string(stream_id))
                .body(response_body);
            
            auto resp_buffer = server.send_response(stream_id, resp);
            if (resp_buffer) {
                std::vector<uint8_t> resp_data(resp_buffer->span().begin(), resp_buffer->span().end());
                network.send_packet(resp_data, "服务器->客户端[" + std::to_string(stream_id) + "]");
            }
        }
    });
    
    server.on_data([&](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        std::string data_str(reinterpret_cast<const char*>(data.data()), data.size());
        server_request_bodies[stream_id] += data_str;
        
        if (end_stream && server_requests.find(stream_id) != server_requests.end()) {
            // 处理POST请求
            auto& req = server_requests[stream_id];
            req.body = server_request_bodies[stream_id];
            
            std::cout << "🔄 [Stream " << stream_id << "] 处理POST请求，体大小: " << req.body.size() << "\n";
            
            auto resp = http1::response()
                .status(201)
                .header("Content-Type", "application/json")
                .header("Location", req.target + "/new_id")
                .body(R"({"status": "success", "message": "资源创建成功"})");
            
            auto resp_buffer = server.send_response(stream_id, resp);
            if (resp_buffer) {
                std::vector<uint8_t> resp_data(resp_buffer->span().begin(), resp_buffer->span().end());
                network.send_packet(resp_data, "服务器->客户端[" + std::to_string(stream_id) + "]");
            }
        }
    });
    
    // 3. 发送连接前置
    std::cout << "\n1. 建立HTTP/2连接:\n";
    auto preface = client.get_preface();
    std::vector<uint8_t> preface_data(preface.begin(), preface.end());
    network.send_packet(preface_data, "客户端->服务器[前置]");
    
    // 4. 并发发送多个请求
    std::cout << "\n2. 并发发送多个请求:\n";
    
    struct RequestInfo {
        std::string name;
        method method_type;
        std::string target;
        std::string body;
    };
    
    std::vector<RequestInfo> requests = {
        {"获取用户列表", method::get, "/api/users", ""},
        {"获取产品列表", method::get, "/api/products", ""},
        {"获取订单列表", method::get, "/api/orders", ""},
        {"慢请求测试", method::get, "/api/slow/test", ""},
        {"创建新用户", method::post, "/api/users", R"({"name": "王五", "email": "wangwu@example.com"})"},
        {"创建新产品", method::post, "/api/products", R"({"name": "新产品", "price": 199.99})"},
        {"大数据查询", method::get, "/api/analytics/report?year=2024", ""},
        {"文件上传", method::post, "/api/files", "这是一个测试文件的内容..."},
    };
    
    // 记录开始时间并发送请求
    auto overall_start = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < requests.size(); ++i) {
        const auto& req_info = requests[i];
        uint32_t stream_id = (i + 1) * 2 + 1; // 奇数流ID（客户端）
        
        stream_start_times[stream_id] = std::chrono::steady_clock::now();
        
        auto req = http1::request()
            .method(req_info.method_type)
            .uri(req_info.target)
            .header("User-Agent", "HTTP2-Concurrent-Demo/1.0")
            .header("Accept", "application/json");
        
        if (!req_info.body.empty()) {
            req.header("Content-Type", "application/json").body(req_info.body);
        }
        
        auto req_buffer = client.send_request(req);
        if (req_buffer) {
            std::vector<uint8_t> req_data(req_buffer->span().begin(), req_buffer->span().end());
            network.send_packet(req_data, "客户端->服务器[" + std::to_string(stream_id) + "]");
            std::cout << "📤 [Stream " << stream_id << "] " << req_info.name << "\n";
        }
        
        // 模拟请求之间的小间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // 5. 处理网络数据包交换
    std::cout << "\n3. 处理网络数据包交换:\n";
    
    auto start_time = std::chrono::steady_clock::now();
    size_t completed_streams = 0;
    const size_t total_streams = requests.size();
    
    while (completed_streams < total_streams && 
           std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start_time).count() < 15) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        auto packets = network.receive_available_packets();
        for (const auto& packet_data : packets) {
            // 根据包的方向处理
            // 这里简化处理，实际应该解析帧头确定方向
            auto process_result = server.process(std::span<const uint8_t>(packet_data));
            if (!process_result) {
                // 尝试客户端处理
                process_result = client.process(std::span<const uint8_t>(packet_data));
            }
        }
        
        // 统计完成的流
        size_t current_completed = 0;
        for (const auto& [stream_id, response_body] : stream_responses) {
            if (!response_body.empty()) {
                current_completed++;
            }
        }
        
        if (current_completed > completed_streams) {
            completed_streams = current_completed;
            std::cout << "📊 已完成: " << completed_streams << "/" << total_streams << " 个流\n";
        }
    }
    
    // 6. 统计结果
    std::cout << "\n4. 并发传输统计:\n";
    
    auto overall_end = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(overall_end - overall_start);
    
    std::cout << "   📊 总体统计:\n";
    std::cout << "      - 总请求数: " << requests.size() << "\n";
    std::cout << "      - 完成请求数: " << completed_streams << "\n";
    std::cout << "      - 总耗时: " << total_duration.count() << "ms\n";
    std::cout << "      - 平均每请求: " << (total_duration.count() / (double)requests.size()) << "ms\n";
    
    std::cout << "   📊 各流详细统计:\n";
    for (size_t i = 0; i < requests.size(); ++i) {
        uint32_t stream_id = (i + 1) * 2 + 1;
        const auto& req_info = requests[i];
        
        if (stream_responses.find(stream_id) != stream_responses.end()) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                overall_end - stream_start_times[stream_id]);
            std::cout << "      - Stream " << stream_id << " (" << req_info.name 
                      << "): " << duration.count() << "ms ✅\n";
        } else {
            std::cout << "      - Stream " << stream_id << " (" << req_info.name 
                      << "): 未完成 ❌\n";
        }
    }
    
    auto client_stats = client.get_stats();
    std::cout << "   📈 客户端统计: " << client_stats.frames_processed << " 帧处理\n";
}

int main() {
    std::cout << "HTTP协议综合演示 - 多数据块传输与并发流\n";
    std::cout << "==========================================\n";
    
    try {
        demo_http1_chunked_transfer();
        demo_http2_concurrent_streams();
        
        std::cout << "\n🎉 所有综合示例运行完成!\n";
        std::cout << "\n💡 演示要点总结:\n";
        std::cout << "   • HTTP/1.x: 流式解析处理大型请求，支持网络丢包和延迟\n";
        std::cout << "   • HTTP/2: 多流并发传输，实现真正的多路复用\n";
        std::cout << "   • 网络模拟: 真实网络条件下的数据包传输\n";
        std::cout << "   • 性能统计: 详细的传输时间和完成率分析\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}