#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>

using namespace co::http;

class Http1ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
};

// =============================================================================
// HTTP/1.x 请求解析测试
// =============================================================================

TEST_F(Http1ParserTest, ParseSimpleGetRequest) {
    std::string request_data = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "User-Agent: TestClient/1.0\r\n"
        "Accept: application/json\r\n"
        "\r\n";
    
    auto result = http1::parse_request(request_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& req = result.value();
    
    EXPECT_EQ(req.method_type, method::get);
    EXPECT_EQ(req.target, "/api/users");
    EXPECT_EQ(req.protocol_version, version::http_1_1);
    EXPECT_EQ(req.headers.size(), 3);
    EXPECT_TRUE(req.body.empty());
    
    // 检查特定头部
    bool host_found = false;
    bool user_agent_found = false;
    for (const auto& h : req.headers) {
        if (h.name == "host" && h.value == "api.example.com") {
            host_found = true;
        }
        if (h.name == "user-agent" && h.value == "TestClient/1.0") {
            user_agent_found = true;
        }
    }
    EXPECT_TRUE(host_found);
    EXPECT_TRUE(user_agent_found);
}

TEST_F(Http1ParserTest, ParsePostRequestWithBody) {
    std::string request_data = 
        "POST /api/users HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 45\r\n"
        "\r\n"
        R"({"name": "张三", "email": "zhang@example.com"})";
    
    auto result = http1::parse_request(request_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& req = result.value();
    
    EXPECT_EQ(req.method_type, method::post);
    EXPECT_EQ(req.target, "/api/users");
    EXPECT_EQ(req.body, R"({"name": "张三", "email": "zhang@example.com"})");
    EXPECT_EQ(req.body.size(), 45);
}

TEST_F(Http1ParserTest, ParseAllHttpMethods) {
    std::vector<std::pair<std::string, method>> method_tests = {
        {"GET", method::get},
        {"POST", method::post},
        {"PUT", method::put},
        {"DELETE", method::delete_},
        {"HEAD", method::head},
        {"OPTIONS", method::options},
        {"TRACE", method::trace},
        {"CONNECT", method::connect},
        {"PATCH", method::patch}
    };
    
    for (const auto& [method_str, expected_method] : method_tests) {
        std::string request_data = method_str + " /test HTTP/1.1\r\n"
                                 "Host: test.com\r\n\r\n";
        
        auto result = http1::parse_request(request_data);
        ASSERT_TRUE(result.has_value()) << "Failed to parse " << method_str;
        EXPECT_EQ(result.value().method_type, expected_method) << "Wrong method for " << method_str;
    }
}

TEST_F(Http1ParserTest, ParseComplexUri) {
    std::string request_data = 
        "GET /api/search?q=hello%20world&page=2&limit=50&sort=name&order=desc HTTP/1.1\r\n"
        "Host: search.example.com\r\n"
        "\r\n";
    
    auto result = http1::parse_request(request_data);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().target, "/api/search?q=hello%20world&page=2&limit=50&sort=name&order=desc");
}

TEST_F(Http1ParserTest, ParseRequestWithManyHeaders) {
    std::string request_data = 
        "POST /api/upload HTTP/1.1\r\n"
        "Host: upload.example.com\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        "Accept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
        "Content-Length: 1024\r\n"
        "Connection: keep-alive\r\n"
        "Upgrade-Insecure-Requests: 1\r\n"
        "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9\r\n"
        "X-Requested-With: XMLHttpRequest\r\n"
        "X-Custom-Header: custom-value\r\n"
        "\r\n"
        "multipart body data here...";
    
    auto result = http1::parse_request(request_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& req = result.value();
    
    EXPECT_EQ(req.headers.size(), 11);
    EXPECT_EQ(req.body, "multipart body data here...");
    
    // 验证特定头部
    bool auth_found = false;
    bool custom_found = false;
    for (const auto& h : req.headers) {
        if (h.name == "authorization" && h.value.starts_with("Bearer")) {
            auth_found = true;
        }
        if (h.name == "x-custom-header" && h.value == "custom-value") {
            custom_found = true;
        }
    }
    EXPECT_TRUE(auth_found);
    EXPECT_TRUE(custom_found);
}

// =============================================================================
// HTTP/1.x 响应解析测试
// =============================================================================

TEST_F(Http1ParserTest, ParseSimpleResponse) {
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 17\r\n"
        "Server: TestServer/1.0\r\n"
        "\r\n"
        R"({"status": "ok"})";
    
    auto result = http1::parse_response(response_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& resp = result.value();
    
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.reason_phrase, "OK");
    EXPECT_EQ(resp.protocol_version, version::http_1_1);
    EXPECT_EQ(resp.body, R"({"status": "ok"})");
    EXPECT_EQ(resp.headers.size(), 3);
    
    // 检查Content-Type头部
    bool content_type_found = false;
    for (const auto& h : resp.headers) {
        if (h.name == "content-type" && h.value == "application/json") {
            content_type_found = true;
        }
    }
    EXPECT_TRUE(content_type_found);
}

TEST_F(Http1ParserTest, ParseResponseWithAllStatusCodes) {
    std::vector<std::pair<unsigned int, std::string>> status_tests = {
        {200, "OK"},
        {201, "Created"},
        {204, "No Content"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {500, "Internal Server Error"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"}
    };
    
    for (const auto& [code, reason] : status_tests) {
        std::string response_data = 
            "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        
        auto result = http1::parse_response(response_data);
        ASSERT_TRUE(result.has_value()) << "Failed to parse status " << code;
        
        const auto& resp = result.value();
        EXPECT_EQ(resp.status_code, code);
        EXPECT_EQ(resp.reason_phrase, reason);
    }
}

TEST_F(Http1ParserTest, ParseResponseWithLargeBody) {
    // 生成大响应体
    std::string large_body;
    large_body.reserve(10000);
    for (int i = 0; i < 1000; ++i) {
        large_body += "This is line " + std::to_string(i) + " of test data.\n";
    }
    
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(large_body.size()) + "\r\n"
        "\r\n" + large_body;
    
    auto result = http1::parse_response(response_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& resp = result.value();
    
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body.size(), large_body.size());
    EXPECT_EQ(resp.body, large_body);
}

// =============================================================================
// 边界条件和错误处理测试
// =============================================================================

TEST_F(Http1ParserTest, ParseIncompleteRequest) {
    // 不完整的请求（缺少结束的空行）
    std::string incomplete_data = 
        "GET /test HTTP/1.1\r\n"
        "Host: test.com\r\n";
        // 缺少结束的\r\n
    
    auto result = http1::parse_request(incomplete_data);
    
    // 应该失败或返回需要更多数据的错误
    // 具体行为取决于实现
    if (!result.has_value()) {
        EXPECT_EQ(result.error(), error_code::need_more_data);
    }
}

TEST_F(Http1ParserTest, ParseMalformedRequest) {
    std::string malformed_data = 
        "INVALID REQUEST LINE\r\n"
        "Host: test.com\r\n"
        "\r\n";
    
    auto result = http1::parse_request(malformed_data);
    
    // 应该失败
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
        // 应该是协议错误或无效方法
        EXPECT_TRUE(result.error() == error_code::protocol_error || 
                   result.error() == error_code::invalid_method);
    }
}

TEST_F(Http1ParserTest, ParseRequestWithSpecialCharacters) {
    std::string request_data = 
        "POST /api/测试/数据 HTTP/1.1\r\n"
        "Host: 测试.example.com\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: 50\r\n"
        "\r\n"
        R"({"中文": "测试", "emoji": "🎉", "符号": "©®™"})";
    
    auto result = http1::parse_request(request_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& req = result.value();
    
    EXPECT_EQ(req.target, "/api/测试/数据");
    EXPECT_TRUE(req.body.find("中文") != std::string::npos);
    EXPECT_TRUE(req.body.find("🎉") != std::string::npos);
}

TEST_F(Http1ParserTest, ParseResponseWithCookies) {
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Set-Cookie: session_id=abc123; HttpOnly; Secure\r\n"
        "Set-Cookie: user_pref=theme_dark; Max-Age=86400\r\n"
        "Set-Cookie: csrf_token=xyz789; SameSite=Strict\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";
    
    auto result = http1::parse_response(response_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& resp = result.value();
    
    // 计算Set-Cookie头部数量
    int cookie_count = 0;
    for (const auto& h : resp.headers) {
        if (h.name == "set-cookie") {
            cookie_count++;
        }
    }
    
    EXPECT_EQ(cookie_count, 3);
    EXPECT_EQ(resp.body, "Hello, World!");
}

// =============================================================================
// 流式解析测试
// =============================================================================

TEST_F(Http1ParserTest, StreamingParserBasic) {
    http1::request_parser parser;
    
    std::string request_data = 
        "GET /api/test HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    
    request req;
    auto result = parser.parse(std::string_view(request_data), req);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0); // 应该消费了一些字节
    EXPECT_TRUE(parser.is_complete());
    
    EXPECT_EQ(req.method_type, method::get);
    EXPECT_EQ(req.target, "/api/test");
    EXPECT_EQ(req.body, "hello");
}

TEST_F(Http1ParserTest, StreamingParserIncremental) {
    http1::request_parser parser;
    
    // 分块发送数据
    std::vector<std::string> chunks = {
        "GET /api/test HTTP/1.1\r\n",
        "Host: api.example.com\r\n",
        "Content-Length: 11\r\n",
        "\r\n",
        "hello world"
    };
    
    request req;
    size_t total_consumed = 0;
    
    for (const auto& chunk : chunks) {
        auto result = parser.parse(std::string_view(chunk), req);
        ASSERT_TRUE(result.has_value());
        total_consumed += result.value();
        
        if (parser.is_complete()) {
            break;
        }
    }
    
    EXPECT_TRUE(parser.is_complete());
    EXPECT_EQ(req.method_type, method::get);
    EXPECT_EQ(req.target, "/api/test");
    EXPECT_EQ(req.body, "hello world");
}