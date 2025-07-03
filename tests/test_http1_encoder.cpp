#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>

using namespace co::http;

class Http1EncoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
    
    // 辅助函数：验证编码的请求能被正确解析
    void VerifyRequestRoundTrip(const request& original_req) {
        auto encoded = http1::encode_request(original_req);
        ASSERT_TRUE(encoded.has_value());
        
        auto parsed = http1::parse_request(encoded.value());
        ASSERT_TRUE(parsed.has_value());
        
        const auto& parsed_req = parsed.value();
        EXPECT_EQ(parsed_req.method_, original_req.method_);
        EXPECT_EQ(parsed_req.target_, original_req.target_);
        EXPECT_EQ(parsed_req.version_, original_req.version_);
        EXPECT_EQ(parsed_req.body_, original_req.body_);
        EXPECT_EQ(parsed_req.headers_.size(), original_req.headers_.size());
    }
    
    // 辅助函数：验证编码的响应能被正确解析
    void VerifyResponseRoundTrip(const response& original_resp) {
        auto encoded = http1::encode_response(original_resp);
        ASSERT_TRUE(encoded.has_value());
        
        auto parsed = http1::parse_response(encoded.value());
        ASSERT_TRUE(parsed.has_value());
        
        const auto& parsed_resp = parsed.value();
        EXPECT_EQ(parsed_resp.status_code_, original_resp.status_code_);
        EXPECT_EQ(parsed_resp.version_, original_resp.version_);
        EXPECT_EQ(parsed_resp.body_, original_resp.body_);
        EXPECT_EQ(parsed_resp.headers_.size(), original_resp.headers_.size());
    }
};

// =============================================================================
// HTTP/1.x 请求编码测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeSimpleGetRequest) {
    request req;
    req.method_ = method::get;
    req.target_ = "/api/users";
    req.version_ = version::http_1_1;
    req.headers_ = {
        {"host", "api.example.com"},
        {"user-agent", "TestClient/1.0"},
        {"accept", "application/json"}
    };
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    std::string expected = 
        "GET /api/users HTTP/1.1\r\n"
        "host: api.example.com\r\n"
        "user-agent: TestClient/1.0\r\n"
        "accept: application/json\r\n"
        "\r\n";
    
    EXPECT_EQ(result.value(), expected);
    VerifyRequestRoundTrip(req);
}

TEST_F(Http1EncoderTest, EncodePostRequestWithBody) {
    request req;
    req.method_ = method::post;
    req.target_ = "/api/users";
    req.version_ = version::http_1_1;
    req.body_ = R"({"name": "张三", "email": "zhang@example.com"})";
    req.headers_ = {
        {"host", "api.example.com"},
        {"content-type", "application/json"},
        {"content-length", std::to_string(req.body_.size())}
    };
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 验证包含正确的Content-Length
    EXPECT_TRUE(result.value().find("content-length: " + std::to_string(req.body_.size())) != std::string::npos);
    // 验证包含完整的body
    EXPECT_TRUE(result.value().find(req.body_) != std::string::npos);
    
    VerifyRequestRoundTrip(req);
}

TEST_F(Http1EncoderTest, EncodeAllHttpMethods) {
    std::vector<method> methods = {
        method::get, method::post, method::put, method::delete_,
        method::head, method::options, method::trace, method::connect, method::patch
    };
    
    for (auto m : methods) {
        request req;
        req.method_ = m;
        req.target_ = "/test";
        req.version_ = version::http_1_1;
        req.headers_ = {{"host", "test.com"}};
        
        auto result = http1::encode_request(req);
        ASSERT_TRUE(result.has_value()) << "Failed to encode method " << static_cast<int>(m);
        
        VerifyRequestRoundTrip(req);
    }
}

TEST_F(Http1EncoderTest, EncodeRequestWithManyHeaders) {
    request req;
    req.method_ = method::post;
    req.target_ = "/api/upload";
    req.version_ = version::http_1_1;
    req.body_ = "test data";
    
    // 添加多个头部
    req.headers_ = {
        {"host", "upload.example.com"},
        {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"},
        {"accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
        {"accept-language", "zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3"},
        {"accept-encoding", "gzip, deflate, br"},
        {"content-type", "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW"},
        {"content-length", std::to_string(req.body_.size())},
        {"connection", "keep-alive"},
        {"authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"},
        {"x-requested-with", "XMLHttpRequest"},
        {"x-custom-header", "custom-value"}
    };
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 验证所有头部都存在
    for (const auto& header : req.headers_) {
        std::string header_line = header.name + ": " + header.value;
        EXPECT_TRUE(result.value().find(header_line) != std::string::npos) 
            << "Missing header: " << header_line;
    }
    
    VerifyRequestRoundTrip(req);
}

TEST_F(Http1EncoderTest, EncodeRequestWithLargeBody) {
    request req;
    req.method_ = method::post;
    req.target_ = "/api/bulk-data";
    req.version_ = version::http_1_1;
    
    // 生成大型JSON数据 (约100KB)
    std::string large_body = R"({"data": [)";
    for (int i = 0; i < 1000; ++i) {
        large_body += "\n    {\"id\": " + std::to_string(i) + 
                      ", \"name\": \"Item" + std::to_string(i) + 
                      "\", \"description\": \"Description for item " + std::to_string(i) + 
                      " with some additional text to make it larger\"}";
        if (i < 999) large_body += ",";
    }
    large_body += "\n]}";
    
    req.body_ = large_body;
    req.headers_ = {
        {"host", "api.example.com"},
        {"content-type", "application/json"},
        {"content-length", std::to_string(req.body_.size())}
    };
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 100000); // 应该大于100KB
    
    VerifyRequestRoundTrip(req);
}

// =============================================================================
// HTTP/1.x 响应编码测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeSimpleResponse) {
    response resp;
    resp.version_ = version::http_1_1;
    resp.status_code_ = status_code::ok;
    resp.reason_phrase_ = "OK";
    resp.headers_ = {
        {"content-type", "application/json"},
        {"server", "TestServer/1.0"},
        {"content-length", "17"}
    };
    resp.body_ = R"({"status": "ok"})";
    
    auto result = http1::encode_response(resp);
    ASSERT_TRUE(result.has_value());
    
    std::string expected = 
        "HTTP/1.1 200 OK\r\n"
        "content-type: application/json\r\n"
        "server: TestServer/1.0\r\n"
        "content-length: 17\r\n"
        "\r\n"
        R"({"status": "ok"})";
    
    EXPECT_EQ(result.value(), expected);
    VerifyResponseRoundTrip(resp);
}

TEST_F(Http1EncoderTest, EncodeAllStatusCodes) {
    std::vector<std::pair<status_code, std::string>> status_tests = {
        {status_code::ok, "OK"},
        {status_code::created, "Created"},
        {status_code::accepted, "Accepted"},
        {status_code::no_content, "No Content"},
        {status_code::bad_request, "Bad Request"},
        {status_code::unauthorized, "Unauthorized"},
        {status_code::forbidden, "Forbidden"},
        {status_code::not_found, "Not Found"},
        {status_code::method_not_allowed, "Method Not Allowed"},
        {status_code::internal_server_error, "Internal Server Error"},
        {status_code::bad_gateway, "Bad Gateway"},
        {status_code::service_unavailable, "Service Unavailable"}
    };
    
    for (const auto& [code, reason] : status_tests) {
        response resp;
        resp.version_ = version::http_1_1;
        resp.status_code_ = code;
        resp.reason_phrase_ = reason;
        resp.headers_ = {{"content-length", "0"}};
        
        auto result = http1::encode_response(resp);
        ASSERT_TRUE(result.has_value()) << "Failed to encode status " << static_cast<int>(code);
        
        // 验证状态行
        std::string status_line = "HTTP/1.1 " + std::to_string(static_cast<int>(code)) + " " + reason;
        EXPECT_TRUE(result.value().find(status_line) != std::string::npos);
        
        VerifyResponseRoundTrip(resp);
    }
}

TEST_F(Http1EncoderTest, EncodeResponseWithCookies) {
    response resp;
    resp.version_ = version::http_1_1;
    resp.status_code_ = status_code::ok;
    resp.reason_phrase_ = "OK";
    resp.headers_ = {
        {"content-type", "text/html"},
        {"set-cookie", "session_id=abc123; HttpOnly; Secure; Path=/"},
        {"set-cookie", "user_pref=theme_dark; Max-Age=86400; Path=/"},
        {"set-cookie", "csrf_token=xyz789; SameSite=Strict"},
        {"server", "WebServer/2.0"}
    };
    resp.body_ = "<html><body>Login successful</body></html>";
    
    auto result = http1::encode_response(resp);
    ASSERT_TRUE(result.has_value());
    
    // 验证所有Set-Cookie头部都存在
    size_t cookie_count = 0;
    size_t pos = 0;
    while ((pos = result.value().find("set-cookie:", pos)) != std::string::npos) {
        cookie_count++;
        pos++;
    }
    EXPECT_EQ(cookie_count, 3);
    
    VerifyResponseRoundTrip(resp);
}

// =============================================================================
// 高性能缓冲区编码测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeToBuffer) {
    request req;
    req.method_ = method::get;
    req.target_ = "/api/test";
    req.version_ = version::http_1_1;
    req.headers_ = {{"host", "api.example.com"}};
    
    output_buffer buffer;
    auto result = http1::encode_request(req, buffer);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0); // 应该写入了一些字节
    EXPECT_EQ(buffer.size(), result.value());
    
    // 验证缓冲区内容
    auto view = buffer.view();
    EXPECT_TRUE(view.find("GET /api/test HTTP/1.1") != std::string::npos);
    EXPECT_TRUE(view.find("host: api.example.com") != std::string::npos);
    
    // 零拷贝访问
    auto span = buffer.span();
    EXPECT_EQ(span.size(), buffer.size());
}

TEST_F(Http1EncoderTest, EncodeMultipleToSameBuffer) {
    output_buffer buffer;
    
    // 第一个请求
    request req1;
    req1.method_ = method::get;
    req1.target_ = "/api/users";
    req1.headers_ = {{"host", "api.example.com"}};
    
    auto result1 = http1::encode_request(req1, buffer);
    ASSERT_TRUE(result1.has_value());
    size_t first_size = buffer.size();
    
    // 第二个请求追加到同一个缓冲区
    request req2;
    req2.method_ = method::post;
    req2.target_ = "/api/products";
    req2.headers_ = {{"host", "api.example.com"}};
    req2.body_ = R"({"name": "test"})";
    
    auto result2 = http1::encode_request(req2, buffer);
    ASSERT_TRUE(result2.has_value());
    
    EXPECT_GT(buffer.size(), first_size);
    EXPECT_EQ(buffer.size(), first_size + result2.value());
    
    // 验证两个请求都在缓冲区中
    auto view = buffer.view();
    EXPECT_TRUE(view.find("GET /api/users") != std::string::npos);
    EXPECT_TRUE(view.find("POST /api/products") != std::string::npos);
}

// =============================================================================
// 错误处理和边界条件测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeEmptyRequest) {
    request req; // 默认初始化的空请求
    auto result = http1::encode_request(req);
    
    // 应该能编码，但可能生成无效的HTTP
    // 具体行为取决于实现
    ASSERT_TRUE(result.has_value());
}

TEST_F(Http1EncoderTest, EncodeRequestWithEmptyHeaders) {
    request req;
    req.method_ = method::get;
    req.target_ = "/test";
    req.version_ = version::http_1_1;
    // 没有头部
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 应该只包含请求行和空行
    std::string expected = "GET /test HTTP/1.1\r\n\r\n";
    EXPECT_EQ(result.value(), expected);
}

TEST_F(Http1EncoderTest, EncodeRequestWithSpecialCharacters) {
    request req;
    req.method_ = method::post;
    req.target_ = "/api/测试/数据?查询=值&特殊=字符";
    req.version_ = version::http_1_1;
    req.headers_ = {
        {"host", "测试.example.com"},
        {"x-special-header", "值包含特殊字符: !@#$%^&*()"}
    };
    req.body_ = R"({"中文": "测试", "emoji": "🎉", "特殊符号": "©®™"})";
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 验证特殊字符被正确编码
    EXPECT_TRUE(result.value().find(req.target_) != std::string::npos);
    EXPECT_TRUE(result.value().find(req.body_) != std::string::npos);
}

TEST_F(Http1EncoderTest, EncodeResponseWithLargeBody) {
    response resp;
    resp.version_ = version::http_1_1;
    resp.status_code_ = status_code::ok;
    resp.reason_phrase_ = "OK";
    
    // 生成1MB的响应体
    std::string large_body;
    large_body.reserve(1024 * 1024);
    for (int i = 0; i < 1024; ++i) {
        large_body += std::string(1024, 'A' + (i % 26));
    }
    
    resp.body_ = large_body;
    resp.headers_ = {
        {"content-type", "text/plain"},
        {"content-length", std::to_string(resp.body_.size())}
    };
    
    auto result = http1::encode_response(resp);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 1024 * 1024); // 应该大于1MB
    
    // 验证Content-Length正确
    std::string content_length_header = "content-length: " + std::to_string(large_body.size());
    EXPECT_TRUE(result.value().find(content_length_header) != std::string::npos);
}

TEST_F(Http1EncoderTest, RoundTripWithComplexData) {
    // 测试复杂数据的编码->解析->编码往返
    request original_req;
    original_req.method_ = method::put;
    original_req.target_ = "/api/complex/测试?param1=value1&param2=特殊值";
    original_req.version_ = version::http_1_1;
    original_req.headers_ = {
        {"host", "api.测试.com"},
        {"content-type", "application/json; charset=utf-8"},
        {"authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c"},
        {"x-custom", "值1,值2,值3"},
        {"accept-language", "zh-CN,zh;q=0.9,en;q=0.8"}
    };
    original_req.body_ = R"({
    "用户": {
        "姓名": "张三",
        "邮箱": "zhang@测试.com",
        "元数据": {
            "标签": ["VIP", "企业用户", "活跃"],
            "偏好": {
                "语言": "zh-CN",
                "主题": "深色模式"
            }
        }
    },
    "操作": "更新用户信息",
    "时间戳": "2025-01-01T12:00:00+08:00"
})";
    
    // 第一次编码
    auto encoded1 = http1::encode_request(original_req);
    ASSERT_TRUE(encoded1.has_value());
    
    // 解析
    auto parsed = http1::parse_request(encoded1.value());
    ASSERT_TRUE(parsed.has_value());
    
    // 第二次编码
    auto encoded2 = http1::encode_request(parsed.value());
    ASSERT_TRUE(encoded2.has_value());
    
    // 两次编码结果应该相同
    EXPECT_EQ(encoded1.value(), encoded2.value());
}