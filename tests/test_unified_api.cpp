#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

using namespace co::http;

class UnifiedApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
    
    // 辅助函数：创建测试请求
    request CreateTestRequest() {
        request req;
        req.method_ = method::get;
        req.target_ = "/api/test";
        req.version_ = version::http_1_1;
        req.headers_ = {
            {"host", "api.example.com"},
            {"user-agent", "TestClient/1.0"},
            {"accept", "application/json"}
        };
        return req;
    }
    
    // 辅助函数：创建测试响应
    response CreateTestResponse() {
        response resp;
        resp.version_ = version::http_1_1;
        resp.status_code_ = status_code::ok;
        resp.reason_phrase_ = "OK";
        resp.headers_ = {
            {"content-type", "application/json"},
            {"server", "TestServer/1.0"}
        };
        resp.body_ = R"({"status": "success"})";
        return resp;
    }
};

// =============================================================================
// 统一API基本功能测试
// =============================================================================

TEST_F(UnifiedApiTest, CreateHttpParseInstance) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 基本属性检查
    EXPECT_TRUE(parser->supports_http1());
    EXPECT_TRUE(parser->supports_http2());
}

TEST_F(UnifiedApiTest, ParseHttp1Request) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    std::string request_data = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: application/json\r\n"
        "\r\n";
    
    auto result = parser->parse_request(request_data);
    ASSERT_TRUE(result.has_value());
    
    const auto& req = result.value();
    EXPECT_EQ(req.method_, method::get);
    EXPECT_EQ(req.target_, "/api/users");
    EXPECT_EQ(req.version_, version::http_1_1);
    EXPECT_EQ(req.headers_.size(), 3);
}

TEST_F(UnifiedApiTest, ParseHttp1Response) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 25\r\n"
        "Server: nginx/1.20.1\r\n"
        "\r\n"
        R"({"status": "success"})";
    
    auto result = parser->parse_response(response_data);
    ASSERT_TRUE(result.has_value());
    
    const auto& resp = result.value();
    EXPECT_EQ(resp.version_, version::http_1_1);
    EXPECT_EQ(resp.status_code_, status_code::ok);
    EXPECT_EQ(resp.reason_phrase_, "OK");
    EXPECT_EQ(resp.body_, R"({"status": "success"})");
}

TEST_F(UnifiedApiTest, EncodeHttp1Request) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    auto req = CreateTestRequest();
    auto result = parser->encode_request(req);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 0);
    EXPECT_TRUE(result.value().find("GET /api/test HTTP/1.1") != std::string::npos);
    EXPECT_TRUE(result.value().find("host: api.example.com") != std::string::npos);
}

TEST_F(UnifiedApiTest, EncodeHttp1Response) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    auto resp = CreateTestResponse();
    auto result = parser->encode_response(resp);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 0);
    EXPECT_TRUE(result.value().find("HTTP/1.1 200 OK") != std::string::npos);
    EXPECT_TRUE(result.value().find("content-type: application/json") != std::string::npos);
}

// =============================================================================
// HTTP/1.1 简单API测试
// =============================================================================

TEST_F(UnifiedApiTest, Http1SimpleParseRequest) {
    std::string request_data = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 17\r\n"
        "\r\n"
        R"({"key": "value"})";
    
    auto result = http1::parse_request(request_data);
    ASSERT_TRUE(result.has_value());
    
    const auto& req = result.value();
    EXPECT_EQ(req.method_, method::post);
    EXPECT_EQ(req.target_, "/api/data");
    EXPECT_EQ(req.body_, R"({"key": "value"})");
}

TEST_F(UnifiedApiTest, Http1SimpleEncodeRequest) {
    request req;
    req.method_ = method::put;
    req.target_ = "/api/users/123";
    req.version_ = version::http_1_1;
    req.headers_ = {
        {"host", "api.example.com"},
        {"content-type", "application/json"}
    };
    req.body_ = R"({"name": "Updated Name"})";
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_TRUE(result.value().find("PUT /api/users/123 HTTP/1.1") != std::string::npos);
    EXPECT_TRUE(result.value().find(R"({"name": "Updated Name"})") != std::string::npos);
}

TEST_F(UnifiedApiTest, Http1StreamParser) {
    http1::request_parser parser;
    request req;
    
    // 分块数据
    std::vector<std::string> chunks = {
        "GET /api",
        "/test HTTP/1.1\r\n",
        "Host: api.example.com\r\n",
        "User-Agent: Test\r\n",
        "\r\n"
    };
    
    for (const auto& chunk : chunks) {
        auto result = parser.parse(chunk, req);
        ASSERT_TRUE(result.has_value());
        
        if (parser.is_complete()) {
            break;
        }
    }
    
    EXPECT_TRUE(parser.is_complete());
    EXPECT_EQ(req.method_, method::get);
    EXPECT_EQ(req.target_, "/api/test");
}

// =============================================================================
// HTTP/2 连接API测试
// =============================================================================

TEST_F(UnifiedApiTest, Http2ClientConnection) {
    bool response_received = false;
    bool data_received = false;
    
    auto client = http2::client()
        .on_response([&response_received](uint32_t stream_id, const response& resp, bool end_stream) {
            EXPECT_GT(stream_id, 0);
            EXPECT_EQ(resp.status_code_, status_code::ok);
            response_received = true;
        })
        .on_data([&data_received](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
            EXPECT_GT(stream_id, 0);
            EXPECT_GT(data.size(), 0);
            data_received = true;
        })
        .on_error([](uint32_t stream_id, h2_error_code error) {
            FAIL() << "Unexpected error: " << static_cast<int>(error);
        });
    
    ASSERT_TRUE(client.has_value());
    
    // 发送请求
    auto req = CreateTestRequest();
    auto stream_id = client.value().send_request(req);
    ASSERT_TRUE(stream_id.has_value());
    EXPECT_GT(stream_id.value(), 0);
    EXPECT_EQ(stream_id.value() % 2, 1); // 客户端流ID应该是奇数
}

TEST_F(UnifiedApiTest, Http2ServerConnection) {
    bool request_received = false;
    bool data_received = false;
    
    auto server = http2::server()
        .on_request([&request_received](uint32_t stream_id, const request& req, bool end_stream) {
            EXPECT_GT(stream_id, 0);
            EXPECT_EQ(req.method_, method::get);
            request_received = true;
        })
        .on_data([&data_received](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
            EXPECT_GT(stream_id, 0);
            data_received = true;
        })
        .on_error([](uint32_t stream_id, h2_error_code error) {
            FAIL() << "Unexpected error: " << static_cast<int>(error);
        });
    
    ASSERT_TRUE(server.has_value());
    
    // 发送响应
    auto resp = CreateTestResponse();
    auto result = server.value().send_response(1, resp);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UnifiedApiTest, Http2PushPromise) {
    auto client = http2::client()
        .on_push_promise([](uint32_t stream_id, uint32_t promised_stream_id, const request& req) {
            EXPECT_GT(stream_id, 0);
            EXPECT_GT(promised_stream_id, 0);
            EXPECT_EQ(promised_stream_id % 2, 0); // 推送流ID应该是偶数
        });
    
    ASSERT_TRUE(client.has_value());
    
    // 测试推送承诺（在实际实现中这需要服务器发起）
    // 这里只是验证API可以正确设置回调
}

// =============================================================================
// 统一API构建器测试
// =============================================================================

TEST_F(UnifiedApiTest, UnifiedRequestBuilder) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 使用统一API构建请求
    auto req = parser->request_builder()
        .method(method::post)
        .target("/api/submit")
        .header("host", "api.example.com")
        .header("content-type", "application/json")
        .json_body(R"({"data": "test"})")
        .build();
    
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req.value().method_, method::post);
    EXPECT_EQ(req.value().target_, "/api/submit");
    EXPECT_TRUE(req.value().body_.find("test") != std::string::npos);
}

TEST_F(UnifiedApiTest, UnifiedResponseBuilder) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 使用统一API构建响应
    auto resp = parser->response_builder()
        .status(status_code::created)
        .header("location", "/api/users/123")
        .json_body(R"({"id": 123, "created": true})")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().status_code_, status_code::created);
    EXPECT_EQ(resp.value().reason_phrase_, "Created");
    
    // 验证Location头部
    bool location_found = false;
    for (const auto& h : resp.value().headers_) {
        if (h.name == "location" && h.value == "/api/users/123") {
            location_found = true;
        }
    }
    EXPECT_TRUE(location_found);
}

// =============================================================================
// 协议版本检测和转换测试
// =============================================================================

TEST_F(UnifiedApiTest, ProtocolVersionDetection) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // HTTP/1.1 请求
    std::string http1_request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    auto version1 = parser->detect_version(http1_request);
    ASSERT_TRUE(version1.has_value());
    EXPECT_EQ(version1.value(), version::http_1_1);
    
    // HTTP/2连接前置
    std::string http2_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    auto version2 = parser->detect_version(http2_preface);
    ASSERT_TRUE(version2.has_value());
    EXPECT_EQ(version2.value(), version::http_2_0);
}

TEST_F(UnifiedApiTest, ProtocolUpgrade) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // HTTP/1.1升级到HTTP/2
    std::string upgrade_request = 
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: Upgrade, HTTP2-Settings\r\n"
        "Upgrade: h2c\r\n"
        "HTTP2-Settings: AAMAAABkAARAAAAAAAIAAAAA\r\n"
        "\r\n";
    
    auto upgrade_info = parser->parse_upgrade_request(upgrade_request);
    ASSERT_TRUE(upgrade_info.has_value());
    EXPECT_EQ(upgrade_info.value().target_version, version::http_2_0);
    EXPECT_TRUE(upgrade_info.value().is_valid);
}

// =============================================================================
// 错误处理和边界条件测试
// =============================================================================

TEST_F(UnifiedApiTest, InvalidRequestHandling) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 无效的HTTP请求
    std::string invalid_request = "INVALID REQUEST\r\n\r\n";
    auto result = parser->parse_request(invalid_request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(UnifiedApiTest, IncompleteDataHandling) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 不完整的HTTP请求
    std::string incomplete_request = "GET /api/test HTTP/1.1\r\nHost: example.com";
    auto result = parser->parse_request(incomplete_request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(UnifiedApiTest, LargeDataHandling) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 创建大的请求体
    std::string large_body(1024 * 1024, 'X'); // 1MB
    
    request req;
    req.method_ = method::post;
    req.target_ = "/api/large-upload";
    req.version_ = version::http_1_1;
    req.headers_ = {
        {"host", "upload.example.com"},
        {"content-type", "application/octet-stream"},
        {"content-length", std::to_string(large_body.size())}
    };
    req.body_ = large_body;
    
    auto encoded = parser->encode_request(req);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_GT(encoded.value().size(), 1024 * 1024);
    
    // 解析回来验证
    auto parsed = parser->parse_request(encoded.value());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value().body_.size(), large_body.size());
}

// =============================================================================
// 并发和线程安全测试
// =============================================================================

TEST_F(UnifiedApiTest, ConcurrentParsing) {
    const int num_threads = 4;
    const int requests_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            auto parser = http_parse::create(); // 每个线程创建自己的解析器
            
            for (int i = 0; i < requests_per_thread; ++i) {
                std::string request = 
                    "GET /api/test/" + std::to_string(t) + "/" + std::to_string(i) + " HTTP/1.1\r\n"
                    "Host: api.example.com\r\n"
                    "User-Agent: Thread" + std::to_string(t) + "\r\n"
                    "\r\n";
                
                auto result = parser->parse_request(request);
                if (result.has_value()) {
                    success_count++;
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(success_count.load(), num_threads * requests_per_thread);
}

TEST_F(UnifiedApiTest, ConcurrentHttp2Streams) {
    auto client = http2::client()
        .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
            // 处理响应
        })
        .on_error([](uint32_t stream_id, h2_error_code error) {
            FAIL() << "Stream error: " << static_cast<int>(error);
        });
    
    ASSERT_TRUE(client.has_value());
    
    // 同时发送多个请求
    std::vector<std::expected<uint32_t, h2_error_code>> stream_ids;
    
    for (int i = 0; i < 10; ++i) {
        auto req = CreateTestRequest();
        req.target_ = "/api/concurrent/" + std::to_string(i);
        
        auto stream_id = client.value().send_request(req);
        stream_ids.push_back(stream_id);
    }
    
    // 验证所有流都成功创建
    for (const auto& stream_id : stream_ids) {
        ASSERT_TRUE(stream_id.has_value());
        EXPECT_GT(stream_id.value(), 0);
    }
}

// =============================================================================
// 性能和内存管理测试
// =============================================================================

TEST_F(UnifiedApiTest, MemoryUsageOptimization) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 创建大量请求以测试内存使用
    const int num_requests = 1000;
    
    for (int i = 0; i < num_requests; ++i) {
        auto req = CreateTestRequest();
        req.target_ = "/api/test/" + std::to_string(i);
        
        auto encoded = parser->encode_request(req);
        ASSERT_TRUE(encoded.has_value());
        
        auto parsed = parser->parse_request(encoded.value());
        ASSERT_TRUE(parsed.has_value());
        
        // 验证往返正确性
        EXPECT_EQ(parsed.value().target_, req.target_);
    }
}

TEST_F(UnifiedApiTest, StreamingPerformance) {
    http1::request_parser parser;
    
    // 模拟流式处理大量数据
    const int chunk_size = 1024;
    const int num_chunks = 1000;
    
    std::string header = 
        "POST /api/stream HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: " + std::to_string(chunk_size * num_chunks) + "\r\n"
        "\r\n";
    
    request req;
    auto result = parser.parse(header, req);
    ASSERT_TRUE(result.has_value());
    
    // 流式处理body
    std::string chunk_data(chunk_size, 'S');
    for (int i = 0; i < num_chunks; ++i) {
        result = parser.parse(chunk_data, req);
        ASSERT_TRUE(result.has_value());
    }
    
    EXPECT_TRUE(parser.is_complete());
    EXPECT_EQ(req.body_.size(), chunk_size * num_chunks);
}

// =============================================================================
// 兼容性和标准符合性测试
// =============================================================================

TEST_F(UnifiedApiTest, HttpStandardCompliance) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 测试各种HTTP方法
    std::vector<method> methods = {
        method::get, method::post, method::put, method::delete_,
        method::head, method::options, method::trace, method::patch
    };
    
    for (auto m : methods) {
        request req;
        req.method_ = m;
        req.target_ = "/test";
        req.version_ = version::http_1_1;
        req.headers_ = {{"host", "example.com"}};
        
        auto encoded = parser->encode_request(req);
        ASSERT_TRUE(encoded.has_value()) << "Failed for method " << static_cast<int>(m);
        
        auto parsed = parser->parse_request(encoded.value());
        ASSERT_TRUE(parsed.has_value()) << "Failed to parse method " << static_cast<int>(m);
        EXPECT_EQ(parsed.value().method_, m);
    }
}

TEST_F(UnifiedApiTest, HeaderCaseInsensitivity) {
    auto parser = http_parse::create();
    ASSERT_NE(parser, nullptr);
    
    // 测试头部名称大小写不敏感
    std::string request_data = 
        "GET /test HTTP/1.1\r\n"
        "HOST: example.com\r\n"        // 大写
        "Content-Type: text/plain\r\n" // 混合大小写
        "user-agent: TestClient\r\n"   // 小写
        "\r\n";
    
    auto result = parser->parse_request(request_data);
    ASSERT_TRUE(result.has_value());
    
    const auto& req = result.value();
    EXPECT_EQ(req.headers_.size(), 3);
    
    // 验证头部被正确解析（应该规范化为小写）
    bool host_found = false, content_type_found = false, user_agent_found = false;
    for (const auto& h : req.headers_) {
        if (h.name == "host") host_found = true;
        if (h.name == "content-type") content_type_found = true;
        if (h.name == "user-agent") user_agent_found = true;
    }
    
    EXPECT_TRUE(host_found);
    EXPECT_TRUE(content_type_found);
    EXPECT_TRUE(user_agent_found);
}