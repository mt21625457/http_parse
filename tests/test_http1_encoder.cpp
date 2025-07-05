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
        EXPECT_EQ(parsed_req.method_type, original_req.method_type);
        EXPECT_EQ(parsed_req.target, original_req.target);
        EXPECT_EQ(parsed_req.protocol_version, original_req.protocol_version);
        EXPECT_EQ(parsed_req.body, original_req.body);
        EXPECT_EQ(parsed_req.headers.size(), original_req.headers.size());
    }
    
    // 辅助函数：验证编码的响应能被正确解析
    void VerifyResponseRoundTrip(const response& original_resp) {
        auto encoded = http1::encode_response(original_resp);
        ASSERT_TRUE(encoded.has_value());
        
        auto parsed = http1::parse_response(encoded.value());
        ASSERT_TRUE(parsed.has_value());
        
        const auto& parsed_resp = parsed.value();
        EXPECT_EQ(parsed_resp.status_code, original_resp.status_code);
        EXPECT_EQ(parsed_resp.protocol_version, original_resp.protocol_version);
        EXPECT_EQ(parsed_resp.body, original_resp.body);
        EXPECT_EQ(parsed_resp.headers.size(), original_resp.headers.size());
    }
};

// =============================================================================
// HTTP/1.x 请求编码测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeSimpleGetRequest) {
    request req;
    req.method_type = method::get;
    req.target = "/api/users";
    req.protocol_version = version::http_1_1;
    req.add_header("host", "api.example.com");
    req.add_header("user-agent", "TestClient/1.0");
    req.add_header("accept", "application/json");
    
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
    req.method_type = method::post;
    req.target = "/api/users";
    req.protocol_version = version::http_1_1;
    req.body = R"({"name": "张三", "email": "zhang@example.com"})";
    req.add_header("host", "api.example.com");
    req.add_header("content-type", "application/json");
    req.add_header("content-length", std::to_string(req.body.size()));
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 验证包含正确的Content-Length
    EXPECT_TRUE(result.value().find("content-length: " + std::to_string(req.body.size())) != std::string::npos);
    // 验证包含完整的body
    EXPECT_TRUE(result.value().find(req.body) != std::string::npos);
    
    VerifyRequestRoundTrip(req);
}

TEST_F(Http1EncoderTest, EncodeAllHttpMethods) {
    std::vector<method> methods = {
        method::get, method::post, method::put, method::delete_,
        method::head, method::options, method::trace, method::connect, method::patch
    };
    
    for (auto m : methods) {
        request req;
        req.method_type = m;
        req.target = "/test";
        req.protocol_version = version::http_1_1;
        req.add_header("host", "test.com");
        
        auto result = http1::encode_request(req);
        ASSERT_TRUE(result.has_value()) << "Failed to encode method " << static_cast<int>(m);
        
        VerifyRequestRoundTrip(req);
    }
}

TEST_F(Http1EncoderTest, EncodeRequestWithManyHeaders) {
    request req;
    req.method_type = method::post;
    req.target = "/api/upload";
    req.protocol_version = version::http_1_1;
    req.body = "test data";
    
    // 添加多个头部
    req.add_header("host", "upload.example.com");
    req.add_header("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    req.add_header("accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    req.add_header("accept-language", "zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3");
    req.add_header("accept-encoding", "gzip, deflate, br");
    req.add_header("content-type", "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW");
    req.add_header("content-length", std::to_string(req.body.size()));
    req.add_header("connection", "keep-alive");
    req.add_header("authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9");
    req.add_header("x-requested-with", "XMLHttpRequest");
    req.add_header("x-custom-header", "custom-value");
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 验证所有头部都存在
    for (const auto& header : req.headers) {
        std::string header_line = header.name + ": " + header.value;
        EXPECT_TRUE(result.value().find(header_line) != std::string::npos) 
            << "Missing header: " << header_line;
    }
    
    VerifyRequestRoundTrip(req);
}

// =============================================================================
// HTTP/1.x 响应编码测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeSimpleResponse) {
    response resp;
    resp.protocol_version = version::http_1_1;
    resp.status_code = 200; // OK
    resp.reason_phrase = "OK";
    resp.add_header("content-type", "application/json");
    resp.add_header("server", "TestServer/1.0");
    resp.add_header("content-length", "16");
    resp.body = R"({"status": "ok"})";
    
    auto result = http1::encode_response(resp);
    ASSERT_TRUE(result.has_value());
    
    std::string expected = 
        "HTTP/1.1 200 OK\r\n"
        "content-type: application/json\r\n"
        "server: TestServer/1.0\r\n"
        "content-length: 16\r\n"
        "\r\n"
        R"({"status": "ok"})";
    
    EXPECT_EQ(result.value(), expected);
    VerifyResponseRoundTrip(resp);
}

TEST_F(Http1EncoderTest, EncodeAllStatusCodes) {
    std::vector<std::pair<unsigned int, std::string>> status_tests = {
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {204, "No Content"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {500, "Internal Server Error"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"}
    };
    
    for (const auto& [code, reason] : status_tests) {
        response resp;
        resp.protocol_version = version::http_1_1;
        resp.status_code = code;
        resp.reason_phrase = reason;
        resp.add_header("content-length", "0");
        
        auto result = http1::encode_response(resp);
        ASSERT_TRUE(result.has_value()) << "Failed to encode status " << code;
        
        // 验证状态行
        std::string status_line = "HTTP/1.1 " + std::to_string(code) + " " + reason;
        EXPECT_TRUE(result.value().find(status_line) != std::string::npos);
        
        VerifyResponseRoundTrip(resp);
    }
}

// =============================================================================
// 高性能缓冲区编码测试
// =============================================================================

TEST_F(Http1EncoderTest, EncodeToBuffer) {
    request req;
    req.method_type = method::get;
    req.target = "/api/test";
    req.protocol_version = version::http_1_1;
    req.add_header("host", "api.example.com");
    
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
    req.method_type = method::get;
    req.target = "/test";
    req.protocol_version = version::http_1_1;
    // 没有头部
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 应该只包含请求行和空行
    std::string expected = "GET /test HTTP/1.1\r\n\r\n";
    EXPECT_EQ(result.value(), expected);
}

TEST_F(Http1EncoderTest, EncodeRequestWithSpecialCharacters) {
    request req;
    req.method_type = method::post;
    req.target = "/api/测试/数据?查询=值&特殊=字符";
    req.protocol_version = version::http_1_1;
    req.add_header("host", "测试.example.com");
    req.add_header("x-special-header", "值包含特殊字符: !@#$%^&*()");
    req.body = R"({"中文": "测试", "emoji": "🎉", "特殊符号": "©®™"})";
    
    auto result = http1::encode_request(req);
    ASSERT_TRUE(result.has_value());
    
    // 验证特殊字符被正确编码
    EXPECT_TRUE(result.value().find(req.target) != std::string::npos);
    EXPECT_TRUE(result.value().find(req.body) != std::string::npos);
}