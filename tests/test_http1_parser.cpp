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
    
    EXPECT_EQ(req.method_, method::get);
    EXPECT_EQ(req.target_, "/api/users");
    EXPECT_EQ(req.version_, version::http_1_1);
    EXPECT_EQ(req.headers_.size(), 3);
    EXPECT_TRUE(req.body_.empty());
    
    // 检查特定头部
    bool host_found = false;
    bool user_agent_found = false;
    for (const auto& h : req.headers_) {
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
    
    EXPECT_EQ(req.method_, method::post);
    EXPECT_EQ(req.target_, "/api/users");
    EXPECT_EQ(req.body_, R"({"name": "张三", "email": "zhang@example.com"})");
    EXPECT_EQ(req.body_.size(), 45);
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
        EXPECT_EQ(result.value().method_, expected_method) << "Wrong method for " << method_str;
    }
}

TEST_F(Http1ParserTest, ParseComplexUri) {
    std::string request_data = 
        "GET /api/search?q=hello%20world&page=2&limit=50&sort=name&order=desc HTTP/1.1\r\n"
        "Host: search.example.com\r\n"
        "\r\n";
    
    auto result = http1::parse_request(request_data);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().target_, "/api/search?q=hello%20world&page=2&limit=50&sort=name&order=desc");
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
    
    EXPECT_EQ(req.headers_.size(), 11);
    EXPECT_EQ(req.body_, "multipart body data here...");
    
    // 验证特定头部
    bool auth_found = false;
    bool custom_found = false;
    for (const auto& h : req.headers_) {
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
        "Content-Length: 25\r\n"
        "Server: nginx/1.20.1\r\n"
        "\r\n"
        R"({"status": "success"})";
    
    auto result = http1::parse_response(response_data);
    
    ASSERT_TRUE(result.has_value());
    const auto& resp = result.value();
    
    EXPECT_EQ(resp.version_, version::http_1_1);
    EXPECT_EQ(resp.status_code_, status_code::ok);
    EXPECT_EQ(resp.reason_phrase_, "OK");
    EXPECT_EQ(resp.headers_.size(), 3);
    EXPECT_EQ(resp.body_, R"({"status": "success"})");
}

TEST_F(Http1ParserTest, ParseAllStatusCodes) {
    std::vector<std::pair<int, status_code>> status_tests = {
        {200, status_code::ok},
        {201, status_code::created},
        {400, status_code::bad_request},
        {401, status_code::unauthorized},
        {403, status_code::forbidden},
        {404, status_code::not_found},
        {500, status_code::internal_server_error},
        {502, status_code::bad_gateway},
        {503, status_code::service_unavailable}
    };
    
    for (const auto& [code_num, expected_code] : status_tests) {
        std::string response_data = "HTTP/1.1 " + std::to_string(code_num) + " Test\r\n"
                                  "Content-Length: 0\r\n\r\n";
        
        auto result = http1::parse_response(response_data);
        ASSERT_TRUE(result.has_value()) << "Failed to parse status " << code_num;
        EXPECT_EQ(result.value().status_code_, expected_code) << "Wrong status for " << code_num;
    }
}

// =============================================================================
// HTTP/1.x 流式解析测试
// =============================================================================

TEST_F(Http1ParserTest, IncrementalParsing) {
    http1::request_parser parser;
    request req;
    
    // 分块发送数据
    std::vector<std::string> chunks = {
        "GET /api",
        "/users/123 HTTP/1.1\r\n",
        "Host: api.example.com\r\n",
        "User-Agent: Test",
        "Client/1.0\r\n",
        "Accept: application/json\r\n",
        "\r\n"
    };
    
    size_t total_parsed = 0;
    for (const auto& chunk : chunks) {
        auto result = parser.parse(chunk, req);
        ASSERT_TRUE(result.has_value());
        total_parsed += result.value();
        
        if (parser.is_complete()) {
            break;
        }
        EXPECT_TRUE(parser.needs_more_data());
    }
    
    EXPECT_TRUE(parser.is_complete());
    EXPECT_FALSE(parser.needs_more_data());
    EXPECT_EQ(req.method_, method::get);
    EXPECT_EQ(req.target_, "/api/users/123");
    EXPECT_EQ(req.headers_.size(), 3);
}

TEST_F(Http1ParserTest, IncrementalParsingWithBody) {
    http1::request_parser parser;
    request req;
    
    std::string large_body = R"({
    "users": [)";
    
    // 添加1000个用户记录
    for (int i = 1; i <= 1000; ++i) {
        large_body += "\n        {\"id\": " + std::to_string(i) + 
                      ", \"name\": \"User" + std::to_string(i) + "\"}";
        if (i < 1000) large_body += ",";
    }
    large_body += "\n    ]\n}";
    
    std::string header = 
        "POST /api/users/bulk HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(large_body.size()) + "\r\n"
        "\r\n";
    
    std::string full_request = header + large_body;
    
    // 分成32字节的块
    const size_t chunk_size = 32;
    for (size_t i = 0; i < full_request.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, full_request.size());
        std::string chunk = full_request.substr(i, end - i);
        
        auto result = parser.parse(chunk, req);
        ASSERT_TRUE(result.has_value());
        
        if (parser.is_complete()) {
            break;
        }
    }
    
    EXPECT_TRUE(parser.is_complete());
    EXPECT_EQ(req.method_, method::post);
    EXPECT_EQ(req.body_.size(), large_body.size());
    EXPECT_EQ(req.body_, large_body);
}

// =============================================================================
// 错误处理测试
// =============================================================================

TEST_F(Http1ParserTest, InvalidMethod) {
    std::string invalid_request = "INVALID /path HTTP/1.1\r\n\r\n";
    auto result = http1::parse_request(invalid_request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(Http1ParserTest, InvalidVersion) {
    std::string invalid_request = "GET /path HTTP/2.5\r\n\r\n";
    auto result = http1::parse_request(invalid_request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(Http1ParserTest, MalformedHeaders) {
    std::string invalid_request = 
        "GET /path HTTP/1.1\r\n"
        "Invalid-Header-Without-Colon\r\n"
        "\r\n";
    auto result = http1::parse_request(invalid_request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(Http1ParserTest, IncompleteRequest) {
    std::string incomplete_request = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 100\r\n"
        "\r\n"
        "only 20 chars here"; // 远少于Content-Length声明的100字节
    
    auto result = http1::parse_request(incomplete_request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(Http1ParserTest, ContentLengthMismatch) {
    std::string mismatch_request = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "this body is much longer than 10 characters";
    
    auto result = http1::parse_request(mismatch_request);
    // 根据实现，这可能成功但只解析前10个字符，或者失败
    // 具体行为取决于解析器实现
}

// =============================================================================
// 边界条件测试
// =============================================================================

TEST_F(Http1ParserTest, EmptyBody) {
    std::string request_data = 
        "GET /api/test HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    
    auto result = http1::parse_request(request_data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().body_.empty());
}

TEST_F(Http1ParserTest, VeryLongHeaders) {
    std::string long_value(8192, 'x'); // 8KB的头部值
    std::string request_data = 
        "GET /api/test HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "X-Very-Long-Header: " + long_value + "\r\n"
        "\r\n";
    
    auto result = http1::parse_request(request_data);
    ASSERT_TRUE(result.has_value());
    
    bool long_header_found = false;
    for (const auto& h : result.value().headers_) {
        if (h.name == "x-very-long-header" && h.value == long_value) {
            long_header_found = true;
            break;
        }
    }
    EXPECT_TRUE(long_header_found);
}

TEST_F(Http1ParserTest, ManyHeaders) {
    std::string request_data = "GET /api/test HTTP/1.1\r\n";
    
    // 添加100个头部
    for (int i = 1; i <= 100; ++i) {
        request_data += "X-Header-" + std::to_string(i) + ": value" + std::to_string(i) + "\r\n";
    }
    request_data += "\r\n";
    
    auto result = http1::parse_request(request_data);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().headers_.size(), 100);
}

TEST_F(Http1ParserTest, ResetParser) {
    http1::request_parser parser;
    request req1, req2;
    
    // 解析第一个请求
    std::string request1 = "GET /api/test1 HTTP/1.1\r\nHost: api.example.com\r\n\r\n";
    auto result1 = parser.parse(request1, req1);
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(parser.is_complete());
    
    // 重置解析器
    parser.reset();
    EXPECT_FALSE(parser.is_complete());
    EXPECT_TRUE(parser.needs_more_data());
    
    // 解析第二个请求
    std::string request2 = "POST /api/test2 HTTP/1.1\r\nHost: api.example.com\r\n\r\n";
    auto result2 = parser.parse(request2, req2);
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(parser.is_complete());
    
    EXPECT_EQ(req1.target_, "/api/test1");
    EXPECT_EQ(req2.target_, "/api/test2");
    EXPECT_EQ(req1.method_, method::get);
    EXPECT_EQ(req2.method_, method::post);
}