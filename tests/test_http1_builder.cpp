#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>

using namespace co::http;

class Http1BuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
    
    // 辅助函数：验证构建的请求
    void VerifyRequest(const request& req, method expected_method, const std::string& expected_target) {
        EXPECT_EQ(req.method_, expected_method);
        EXPECT_EQ(req.target_, expected_target);
        EXPECT_EQ(req.version_, version::http_1_1);
    }
    
    // 辅助函数：验证构建的响应
    void VerifyResponse(const response& resp, status_code expected_status, const std::string& expected_reason) {
        EXPECT_EQ(resp.status_code_, expected_status);
        EXPECT_EQ(resp.reason_phrase_, expected_reason);
        EXPECT_EQ(resp.version_, version::http_1_1);
    }
};

// =============================================================================
// HTTP/1.x 请求构建器测试
// =============================================================================

TEST_F(Http1BuilderTest, BuildSimpleGetRequest) {
    auto req = http1::request_builder()
        .method(method::get)
        .target("/api/users")
        .header("host", "api.example.com")
        .header("user-agent", "TestClient/1.0")
        .build();
    
    ASSERT_TRUE(req.has_value());
    VerifyRequest(req.value(), method::get, "/api/users");
    
    // 验证头部
    EXPECT_EQ(req.value().headers_.size(), 2);
    bool host_found = false, ua_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "host" && h.value == "api.example.com") host_found = true;
        if (h.name == "user-agent" && h.value == "TestClient/1.0") ua_found = true;
    }
    EXPECT_TRUE(host_found);
    EXPECT_TRUE(ua_found);
}

TEST_F(Http1BuilderTest, BuildPostRequestWithBody) {
    std::string json_body = R"({"name": "张三", "email": "zhang@example.com"})";
    
    auto req = http1::request_builder()
        .method(method::post)
        .target("/api/users")
        .header("host", "api.example.com")
        .header("content-type", "application/json")
        .body(json_body)
        .build();
    
    ASSERT_TRUE(req.has_value());
    VerifyRequest(req.value(), method::post, "/api/users");
    EXPECT_EQ(req.value().body_, json_body);
    
    // 验证Content-Length是否自动添加
    bool content_length_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "content-length") {
            EXPECT_EQ(h.value, std::to_string(json_body.size()));
            content_length_found = true;
        }
    }
    EXPECT_TRUE(content_length_found);
}

TEST_F(Http1BuilderTest, BuildRequestWithMultipleHeaders) {
    auto req = http1::request_builder()
        .method(method::get)
        .target("/api/search")
        .header("host", "search.example.com")
        .header("user-agent", "Mozilla/5.0")
        .header("accept", "application/json")
        .header("accept-language", "zh-CN,zh;q=0.9")
        .header("accept-encoding", "gzip, deflate")
        .header("connection", "keep-alive")
        .header("authorization", "Bearer token123")
        .build();
    
    ASSERT_TRUE(req.has_value());
    VerifyRequest(req.value(), method::get, "/api/search");
    EXPECT_EQ(req.value().headers_.size(), 7);
    
    // 验证特定头部
    bool auth_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "authorization" && h.value == "Bearer token123") {
            auth_found = true;
        }
    }
    EXPECT_TRUE(auth_found);
}

TEST_F(Http1BuilderTest, BuildRequestWithQueryParameters) {
    auto req = http1::request_builder()
        .method(method::get)
        .target("/api/search")
        .query_param("q", "hello world")
        .query_param("page", "2")
        .query_param("limit", "50")
        .query_param("sort", "name")
        .header("host", "api.example.com")
        .build();
    
    ASSERT_TRUE(req.has_value());
    
    // 验证查询参数被正确编码到target中
    std::string expected_target = "/api/search?q=hello%20world&page=2&limit=50&sort=name";
    EXPECT_EQ(req.value().target_, expected_target);
}

TEST_F(Http1BuilderTest, BuildRequestWithFormData) {
    auto req = http1::request_builder()
        .method(method::post)
        .target("/api/login")
        .header("host", "auth.example.com")
        .form_data("username", "testuser")
        .form_data("password", "testpass")
        .form_data("remember", "true")
        .build();
    
    ASSERT_TRUE(req.has_value());
    VerifyRequest(req.value(), method::post, "/api/login");
    
    // 验证Content-Type是否设置为application/x-www-form-urlencoded
    bool content_type_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "content-type" && h.value == "application/x-www-form-urlencoded") {
            content_type_found = true;
        }
    }
    EXPECT_TRUE(content_type_found);
    
    // 验证body包含表单数据
    std::string expected_body = "username=testuser&password=testpass&remember=true";
    EXPECT_EQ(req.value().body_, expected_body);
}

TEST_F(Http1BuilderTest, BuildRequestWithBasicAuth) {
    auto req = http1::request_builder()
        .method(method::get)
        .target("/api/protected")
        .header("host", "api.example.com")
        .basic_auth("username", "password")
        .build();
    
    ASSERT_TRUE(req.has_value());
    
    // 验证Authorization头部
    bool auth_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "authorization" && h.value.starts_with("Basic ")) {
            auth_found = true;
        }
    }
    EXPECT_TRUE(auth_found);
}

TEST_F(Http1BuilderTest, BuildRequestWithBearerToken) {
    auto req = http1::request_builder()
        .method(method::get)
        .target("/api/user/profile")
        .header("host", "api.example.com")
        .bearer_token("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9")
        .build();
    
    ASSERT_TRUE(req.has_value());
    
    // 验证Authorization头部
    bool auth_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "authorization" && h.value == "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9") {
            auth_found = true;
        }
    }
    EXPECT_TRUE(auth_found);
}

// =============================================================================
// HTTP/1.x 响应构建器测试
// =============================================================================

TEST_F(Http1BuilderTest, BuildSimpleResponse) {
    auto resp = http1::response_builder()
        .status(status_code::ok)
        .header("content-type", "application/json")
        .header("server", "TestServer/1.0")
        .body(R"({"message": "success"})")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    VerifyResponse(resp.value(), status_code::ok, "OK");
    EXPECT_EQ(resp.value().body_, R"({"message": "success"})");
    
    // 验证头部
    EXPECT_EQ(resp.value().headers_.size(), 3); // 包含自动添加的Content-Length
    
    bool content_type_found = false, server_found = false;
    for (const auto& h : resp.value().headers_) {
        if (h.name == "content-type" && h.value == "application/json") content_type_found = true;
        if (h.name == "server" && h.value == "TestServer/1.0") server_found = true;
    }
    EXPECT_TRUE(content_type_found);
    EXPECT_TRUE(server_found);
}

TEST_F(Http1BuilderTest, BuildResponseWithAllStatusCodes) {
    std::vector<std::pair<status_code, std::string>> status_tests = {
        {status_code::ok, "OK"},
        {status_code::created, "Created"},
        {status_code::accepted, "Accepted"},
        {status_code::no_content, "No Content"},
        {status_code::bad_request, "Bad Request"},
        {status_code::unauthorized, "Unauthorized"},
        {status_code::forbidden, "Forbidden"},
        {status_code::not_found, "Not Found"},
        {status_code::internal_server_error, "Internal Server Error"},
        {status_code::service_unavailable, "Service Unavailable"}
    };
    
    for (const auto& [code, reason] : status_tests) {
        auto resp = http1::response_builder()
            .status(code)
            .body("Test")
            .build();
        
        ASSERT_TRUE(resp.has_value()) << "Failed to build response for status " << static_cast<int>(code);
        VerifyResponse(resp.value(), code, reason);
    }
}

TEST_F(Http1BuilderTest, BuildResponseWithCookies) {
    auto resp = http1::response_builder()
        .status(status_code::ok)
        .header("content-type", "text/html")
        .cookie("session_id", "abc123", 
               {.http_only = true, .secure = true, .path = "/"})
        .cookie("user_pref", "theme_dark", 
               {.max_age = 86400, .path = "/", .same_site = "Lax"})
        .cookie("csrf_token", "xyz789", 
               {.same_site = "Strict"})
        .body("<html><body>Success</body></html>")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    
    // 验证Set-Cookie头部
    int cookie_count = 0;
    for (const auto& h : resp.value().headers_) {
        if (h.name == "set-cookie") {
            cookie_count++;
        }
    }
    EXPECT_EQ(cookie_count, 3);
}

TEST_F(Http1BuilderTest, BuildResponseWithCustomHeaders) {
    auto resp = http1::response_builder()
        .status(status_code::ok)
        .header("access-control-allow-origin", "*")
        .header("access-control-allow-methods", "GET, POST, PUT, DELETE")
        .header("access-control-allow-headers", "Content-Type, Authorization")
        .header("cache-control", "no-cache, no-store, must-revalidate")
        .header("x-content-type-options", "nosniff")
        .header("x-frame-options", "DENY")
        .header("x-xss-protection", "1; mode=block")
        .body("API response")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().headers_.size(), 8); // 7个自定义头部 + Content-Length
    
    // 验证CORS头部
    bool cors_origin_found = false;
    for (const auto& h : resp.value().headers_) {
        if (h.name == "access-control-allow-origin" && h.value == "*") {
            cors_origin_found = true;
        }
    }
    EXPECT_TRUE(cors_origin_found);
}

TEST_F(Http1BuilderTest, BuildJsonResponse) {
    auto resp = http1::response_builder()
        .status(status_code::ok)
        .json_body(R"({"users": [{"id": 1, "name": "张三"}, {"id": 2, "name": "李四"}]})")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    
    // 验证Content-Type自动设置为application/json
    bool json_content_type_found = false;
    for (const auto& h : resp.value().headers_) {
        if (h.name == "content-type" && h.value == "application/json") {
            json_content_type_found = true;
        }
    }
    EXPECT_TRUE(json_content_type_found);
}

TEST_F(Http1BuilderTest, BuildHtmlResponse) {
    auto resp = http1::response_builder()
        .status(status_code::ok)
        .html_body("<html><head><title>Test</title></head><body><h1>Hello</h1></body></html>")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    
    // 验证Content-Type自动设置为text/html
    bool html_content_type_found = false;
    for (const auto& h : resp.value().headers_) {
        if (h.name == "content-type" && h.value == "text/html; charset=utf-8") {
            html_content_type_found = true;
        }
    }
    EXPECT_TRUE(html_content_type_found);
}

// =============================================================================
// HTTP/1.x 构建器链式调用测试
// =============================================================================

TEST_F(Http1BuilderTest, RequestBuilderChaining) {
    auto req = http1::request_builder()
        .method(method::post)
        .target("/api/upload")
        .header("host", "upload.example.com")
        .header("user-agent", "TestClient/2.0")
        .header("accept", "application/json")
        .query_param("type", "image")
        .query_param("format", "png")
        .form_data("name", "test.png")
        .form_data("size", "1024")
        .basic_auth("user", "pass")
        .build();
    
    ASSERT_TRUE(req.has_value());
    
    // 验证所有设置都生效
    EXPECT_EQ(req.value().method_, method::post);
    EXPECT_TRUE(req.value().target_.find("?type=image&format=png") != std::string::npos);
    EXPECT_TRUE(req.value().body_.find("name=test.png&size=1024") != std::string::npos);
    EXPECT_GT(req.value().headers_.size(), 5);
}

TEST_F(Http1BuilderTest, ResponseBuilderChaining) {
    auto resp = http1::response_builder()
        .status(status_code::created)
        .header("location", "/api/users/123")
        .header("x-request-id", "abc-123-def")
        .cookie("session", "new_session_id", {.http_only = true})
        .json_body(R"({"id": 123, "name": "新用户", "created_at": "2025-01-01T12:00:00Z"})")
        .build();
    
    ASSERT_TRUE(resp.has_value());
    
    // 验证所有设置都生效
    EXPECT_EQ(resp.value().status_code_, status_code::created);
    EXPECT_EQ(resp.value().reason_phrase_, "Created");
    EXPECT_GT(resp.value().headers_.size(), 4);
    
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
// HTTP/1.x 构建器错误处理测试
// =============================================================================

TEST_F(Http1BuilderTest, BuildRequestWithoutRequiredFields) {
    // 没有设置method的请求
    auto req = http1::request_builder()
        .target("/api/test")
        .header("host", "api.example.com")
        .build();
    
    // 根据实现，这可能成功（使用默认值）或失败
    // 具体行为取决于builder的实现策略
}

TEST_F(Http1BuilderTest, BuildResponseWithoutRequiredFields) {
    // 没有设置状态码的响应
    auto resp = http1::response_builder()
        .header("content-type", "text/plain")
        .body("test")
        .build();
    
    // 根据实现，这可能成功（使用默认值）或失败
    // 具体行为取决于builder的实现策略
}

TEST_F(Http1BuilderTest, BuildRequestWithInvalidData) {
    // 尝试添加无效的头部
    auto req = http1::request_builder()
        .method(method::get)
        .target("/api/test")
        .header("", "invalid_empty_name") // 空的头部名称
        .header("valid-header", "")       // 空的头部值（通常是有效的）
        .build();
    
    // 根据实现的验证策略，这可能成功或失败
    // 严格的实现应该拒绝无效的头部
}

// =============================================================================
// HTTP/1.x 构建器性能测试
// =============================================================================

TEST_F(Http1BuilderTest, BuildManyRequests) {
    // 构建大量请求来测试性能
    const int num_requests = 1000;
    
    for (int i = 0; i < num_requests; ++i) {
        auto req = http1::request_builder()
            .method(method::get)
            .target("/api/item/" + std::to_string(i))
            .header("host", "api.example.com")
            .header("user-agent", "BenchmarkClient/1.0")
            .header("x-request-id", "req_" + std::to_string(i))
            .query_param("id", std::to_string(i))
            .build();
        
        ASSERT_TRUE(req.has_value()) << "Failed to build request " << i;
        EXPECT_EQ(req.value().target_, "/api/item/" + std::to_string(i) + "?id=" + std::to_string(i));
    }
}

TEST_F(Http1BuilderTest, BuildRequestWithLargeBody) {
    // 构建带有大body的请求
    std::string large_body;
    large_body.reserve(1024 * 1024); // 1MB
    
    for (int i = 0; i < 1024; ++i) {
        large_body += std::string(1024, 'A' + (i % 26));
    }
    
    auto req = http1::request_builder()
        .method(method::post)
        .target("/api/bulk-upload")
        .header("host", "upload.example.com")
        .header("content-type", "application/octet-stream")
        .body(large_body)
        .build();
    
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req.value().body_.size(), 1024 * 1024);
    
    // 验证Content-Length正确
    bool content_length_found = false;
    for (const auto& h : req.value().headers_) {
        if (h.name == "content-length") {
            EXPECT_EQ(h.value, std::to_string(large_body.size()));
            content_length_found = true;
        }
    }
    EXPECT_TRUE(content_length_found);
}

// =============================================================================
// HTTP/1.x 构建器复制和重用测试
// =============================================================================

TEST_F(Http1BuilderTest, BuilderReuse) {
    auto builder = http1::request_builder()
        .method(method::get)
        .header("host", "api.example.com")
        .header("user-agent", "TestClient/1.0");
    
    // 构建第一个请求
    auto req1 = builder.target("/api/users").build();
    ASSERT_TRUE(req1.has_value());
    EXPECT_EQ(req1.value().target_, "/api/users");
    
    // 重用builder构建第二个请求
    auto req2 = builder.target("/api/products").build();
    ASSERT_TRUE(req2.has_value());
    EXPECT_EQ(req2.value().target_, "/api/products");
    
    // 两个请求应该有相同的基本头部
    EXPECT_EQ(req1.value().headers_.size(), req2.value().headers_.size());
}

TEST_F(Http1BuilderTest, BuilderCopy) {
    auto base_builder = http1::request_builder()
        .method(method::get)
        .header("host", "api.example.com")
        .header("user-agent", "TestClient/1.0")
        .header("accept", "application/json");
    
    // 复制builder并添加认证
    auto auth_builder = base_builder.bearer_token("token123");
    auto auth_req = auth_builder.target("/api/protected").build();
    
    // 从原builder构建无认证请求
    auto public_req = base_builder.target("/api/public").build();
    
    ASSERT_TRUE(auth_req.has_value());
    ASSERT_TRUE(public_req.has_value());
    
    // 验证认证请求有Authorization头部
    bool auth_found = false;
    for (const auto& h : auth_req.value().headers_) {
        if (h.name == "authorization") {
            auth_found = true;
        }
    }
    EXPECT_TRUE(auth_found);
    
    // 验证公共请求没有Authorization头部
    bool no_auth = true;
    for (const auto& h : public_req.value().headers_) {
        if (h.name == "authorization") {
            no_auth = false;
        }
    }
    EXPECT_TRUE(no_auth);
}