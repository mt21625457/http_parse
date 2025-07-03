#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>

using namespace co::http;
using namespace std::chrono;

class BenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
    
    // 辅助函数：测量执行时间
    template<typename Func>
    double MeasureTime(Func&& func, int iterations = 1000) {
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        return static_cast<double>(duration.count()) / iterations; // 平均每次执行的微秒数
    }
    
    // 辅助函数：生成随机字符串
    std::string GenerateRandomString(size_t length) {
        static const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, charset.size() - 1);
        
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += charset[dis(gen)];
        }
        
        return result;
    }
    
    // 辅助函数：创建测试数据
    std::vector<std::string> CreateTestRequests(int count) {
        std::vector<std::string> requests;
        requests.reserve(count);
        
        std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE", "HEAD"};
        std::vector<std::string> paths = {"/", "/api/users", "/api/data", "/upload", "/download"};
        std::vector<std::string> hosts = {"api.example.com", "www.example.com", "cdn.example.com"};
        
        for (int i = 0; i < count; ++i) {
            std::string method = methods[i % methods.size()];
            std::string path = paths[i % paths.size()] + "/" + std::to_string(i);
            std::string host = hosts[i % hosts.size()];
            
            std::string request = 
                method + " " + path + " HTTP/1.1\r\n"
                "Host: " + host + "\r\n"
                "User-Agent: BenchmarkClient/1.0\r\n"
                "Accept: application/json\r\n"
                "Connection: keep-alive\r\n"
                "\r\n";
            
            requests.push_back(request);
        }
        
        return requests;
    }
    
    // 打印性能结果
    void PrintBenchmarkResult(const std::string& test_name, double avg_time_us, int iterations) {
        double ops_per_sec = 1000000.0 / avg_time_us;
        std::cout << "\n=== " << test_name << " ===" << std::endl;
        std::cout << "Iterations: " << iterations << std::endl;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) << avg_time_us << " μs" << std::endl;
        std::cout << "Operations/sec: " << std::fixed << std::setprecision(0) << ops_per_sec << std::endl;
    }
};

// =============================================================================
// HTTP/1.x 解析性能测试
// =============================================================================

TEST_F(BenchmarkTest, Http1ParseRequestBenchmark) {
    const int iterations = 10000;
    auto test_requests = CreateTestRequests(100);
    
    auto avg_time = MeasureTime([&]() {
        static int index = 0;
        const std::string& request = test_requests[index % test_requests.size()];
        index++;
        
        auto result = http1::parse_request(request);
        ASSERT_TRUE(result.has_value());
    }, iterations);
    
    PrintBenchmarkResult("HTTP/1 Request Parsing", avg_time, iterations);
    
    // 性能基准：应该能够在合理时间内解析
    EXPECT_LT(avg_time, 100.0); // 平均每次解析少于100微秒
}

TEST_F(BenchmarkTest, Http1ParseResponseBenchmark) {
    const int iterations = 10000;
    
    std::vector<std::string> responses;
    for (int i = 0; i < 100; ++i) {
        std::string body = R"({"id": )" + std::to_string(i) + R"(, "name": "user)" + std::to_string(i) + R"("})";
        std::string response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Server: BenchmarkServer/1.0\r\n"
            "\r\n" + body;
        responses.push_back(response);
    }
    
    auto avg_time = MeasureTime([&]() {
        static int index = 0;
        const std::string& response = responses[index % responses.size()];
        index++;
        
        auto result = http1::parse_response(response);
        ASSERT_TRUE(result.has_value());
    }, iterations);
    
    PrintBenchmarkResult("HTTP/1 Response Parsing", avg_time, iterations);
    EXPECT_LT(avg_time, 100.0);
}

TEST_F(BenchmarkTest, Http1StreamParsingBenchmark) {
    const int iterations = 1000;
    
    auto avg_time = MeasureTime([&]() {
        http1::request_parser parser;
        request req;
        
        std::vector<std::string> chunks = {
            "GET /api/stream/",
            std::to_string(rand() % 1000),
            " HTTP/1.1\r\n",
            "Host: api.example.com\r\n",
            "User-Agent: StreamClient\r\n",
            "\r\n"
        };
        
        for (const auto& chunk : chunks) {
            auto result = parser.parse(chunk, req);
            ASSERT_TRUE(result.has_value());
            
            if (parser.is_complete()) {
                break;
            }
        }
        
        ASSERT_TRUE(parser.is_complete());
    }, iterations);
    
    PrintBenchmarkResult("HTTP/1 Stream Parsing", avg_time, iterations);
    EXPECT_LT(avg_time, 200.0);
}

// =============================================================================
// HTTP/1.x 编码性能测试
// =============================================================================

TEST_F(BenchmarkTest, Http1EncodeRequestBenchmark) {
    const int iterations = 10000;
    
    std::vector<request> requests;
    for (int i = 0; i < 100; ++i) {
        request req;
        req.method_ = static_cast<method>(static_cast<int>(method::get) + (i % 5));
        req.target_ = "/api/endpoint/" + std::to_string(i);
        req.version_ = version::http_1_1;
        req.headers_ = {
            {"host", "api.example.com"},
            {"user-agent", "BenchmarkClient/1.0"},
            {"accept", "application/json"},
            {"x-request-id", "req_" + std::to_string(i)}
        };
        requests.push_back(req);
    }
    
    auto avg_time = MeasureTime([&]() {
        static int index = 0;
        const request& req = requests[index % requests.size()];
        index++;
        
        auto result = http1::encode_request(req);
        ASSERT_TRUE(result.has_value());
    }, iterations);
    
    PrintBenchmarkResult("HTTP/1 Request Encoding", avg_time, iterations);
    EXPECT_LT(avg_time, 50.0);
}

TEST_F(BenchmarkTest, Http1EncodeResponseBenchmark) {
    const int iterations = 10000;
    
    std::vector<response> responses;
    for (int i = 0; i < 100; ++i) {
        response resp;
        resp.version_ = version::http_1_1;
        resp.status_code_ = status_code::ok;
        resp.reason_phrase_ = "OK";
        resp.headers_ = {
            {"content-type", "application/json"},
            {"server", "BenchmarkServer/1.0"},
            {"x-response-id", "resp_" + std::to_string(i)}
        };
        resp.body_ = R"({"id": )" + std::to_string(i) + R"(, "data": "response_data"})";
        responses.push_back(resp);
    }
    
    auto avg_time = MeasureTime([&]() {
        static int index = 0;
        const response& resp = responses[index % responses.size()];
        index++;
        
        auto result = http1::encode_response(resp);
        ASSERT_TRUE(result.has_value());
    }, iterations);
    
    PrintBenchmarkResult("HTTP/1 Response Encoding", avg_time, iterations);
    EXPECT_LT(avg_time, 50.0);
}

// =============================================================================
// HTTP/2 性能测试
// =============================================================================

TEST_F(BenchmarkTest, Http2FrameProcessingBenchmark) {
    const int iterations = 5000;
    
    // 创建测试帧数据
    std::vector<std::vector<uint8_t>> test_frames;
    for (int i = 0; i < 100; ++i) {
        // 创建DATA帧
        std::string data = "HTTP/2 frame data " + std::to_string(i);
        std::vector<uint8_t> frame(9 + data.size());
        
        // 帧头
        frame[0] = (data.size() >> 16) & 0xFF;
        frame[1] = (data.size() >> 8) & 0xFF;
        frame[2] = data.size() & 0xFF;
        frame[3] = 0x00; // DATA帧
        frame[4] = 0x00; // 无标志
        frame[5] = 0x00;
        frame[6] = 0x00;
        frame[7] = 0x00;
        frame[8] = (i + 1) & 0xFF; // 流ID
        
        // 数据
        std::copy(data.begin(), data.end(), frame.begin() + 9);
        test_frames.push_back(frame);
    }
    
    auto avg_time = MeasureTime([&]() {
        static int index = 0;
        const auto& frame = test_frames[index % test_frames.size()];
        index++;
        
        v2::frame_processor processor;
        bool frame_received = false;
        
        processor.set_data_callback([&frame_received](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
            frame_received = true;
        });
        
        auto result = processor.process_frame(std::span<const uint8_t>(frame));
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(frame_received);
    }, iterations);
    
    PrintBenchmarkResult("HTTP/2 Frame Processing", avg_time, iterations);
    EXPECT_LT(avg_time, 200.0);
}

TEST_F(BenchmarkTest, HpackCompressionBenchmark) {
    const int iterations = 5000;
    
    std::vector<std::vector<header_field>> header_sets;
    for (int i = 0; i < 100; ++i) {
        std::vector<header_field> headers = {
            {":method", "GET"},
            {":scheme", "https"},
            {":path", "/api/data/" + std::to_string(i)},
            {":authority", "api.example.com"},
            {"user-agent", "Mozilla/5.0 (compatible; BenchmarkClient/1.0)"},
            {"accept", "application/json"},
            {"accept-language", "en-US,en;q=0.9"},
            {"accept-encoding", "gzip, deflate, br"},
            {"x-request-id", "req_" + std::to_string(i)}
        };
        header_sets.push_back(headers);
    }
    
    auto avg_time = MeasureTime([&]() {
        static int index = 0;
        static detail::hpack_encoder encoder;
        static detail::hpack_decoder decoder;
        
        const auto& headers = header_sets[index % header_sets.size()];
        index++;
        
        // 编码
        output_buffer buffer;
        auto encode_result = encoder.encode(headers, buffer);
        ASSERT_TRUE(encode_result.has_value());
        
        // 解码
        auto decode_result = decoder.decode(buffer.span());
        ASSERT_TRUE(decode_result.has_value());
        ASSERT_EQ(decode_result.value().size(), headers.size());
    }, iterations);
    
    PrintBenchmarkResult("HPACK Compression/Decompression", avg_time, iterations);
    EXPECT_LT(avg_time, 300.0);
}

// =============================================================================
// 大数据量性能测试
// =============================================================================

TEST_F(BenchmarkTest, LargeBodyParsingBenchmark) {
    const int iterations = 100;
    
    // 创建1MB的请求体
    std::string large_body(1024 * 1024, 'X');
    std::string request = 
        "POST /api/large-upload HTTP/1.1\r\n"
        "Host: upload.example.com\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: " + std::to_string(large_body.size()) + "\r\n"
        "\r\n" + large_body;
    
    auto avg_time = MeasureTime([&]() {
        auto result = http1::parse_request(request);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().body_.size(), large_body.size());
    }, iterations);
    
    PrintBenchmarkResult("Large Body Parsing (1MB)", avg_time, iterations);
    
    // 计算吞吐量 (MB/s)
    double throughput = (1024.0 * 1024.0) / (avg_time / 1000000.0) / (1024.0 * 1024.0);
    std::cout << "Throughput: " << std::fixed << std::setprecision(1) << throughput << " MB/s" << std::endl;
}

TEST_F(BenchmarkTest, LargeBodyEncodingBenchmark) {
    const int iterations = 100;
    
    request req;
    req.method_ = method::post;
    req.target_ = "/api/large-data";
    req.version_ = version::http_1_1;
    req.headers_ = {
        {"host", "api.example.com"},
        {"content-type", "application/json"}
    };
    
    // 创建1MB的JSON数据
    std::string large_json = R"({"data": ")";
    large_json += std::string(1024 * 1024 - 12, 'J'); // 减去JSON结构的字符
    large_json += R"("})";
    req.body_ = large_json;
    
    auto avg_time = MeasureTime([&]() {
        auto result = http1::encode_request(req);
        ASSERT_TRUE(result.has_value());
        ASSERT_GT(result.value().size(), large_json.size());
    }, iterations);
    
    PrintBenchmarkResult("Large Body Encoding (1MB)", avg_time, iterations);
    
    // 计算吞吐量
    double throughput = (1024.0 * 1024.0) / (avg_time / 1000000.0) / (1024.0 * 1024.0);
    std::cout << "Throughput: " << std::fixed << std::setprecision(1) << throughput << " MB/s" << std::endl;
}

// =============================================================================
// 并发性能测试
// =============================================================================

TEST_F(BenchmarkTest, ConcurrentParsingBenchmark) {
    const int num_threads = 4;
    const int iterations_per_thread = 2500;
    const int total_iterations = num_threads * iterations_per_thread;
    
    auto test_requests = CreateTestRequests(100);
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterations_per_thread; ++i) {
                const std::string& request = test_requests[(t * iterations_per_thread + i) % test_requests.size()];
                auto result = http1::parse_request(request);
                ASSERT_TRUE(result.has_value());
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    double avg_time = static_cast<double>(duration.count()) / total_iterations;
    
    PrintBenchmarkResult("Concurrent HTTP/1 Parsing (" + std::to_string(num_threads) + " threads)", avg_time, total_iterations);
    
    // 并发解析应该有良好的性能
    EXPECT_LT(avg_time, 150.0);
}

// =============================================================================
// 内存使用性能测试
// =============================================================================

TEST_F(BenchmarkTest, MemoryAllocationBenchmark) {
    const int iterations = 10000;
    
    // 测试缓冲区重用的性能
    output_buffer reused_buffer;
    
    auto avg_time_reused = MeasureTime([&]() {
        reused_buffer.clear();
        
        request req;
        req.method_ = method::get;
        req.target_ = "/api/test/" + std::to_string(rand() % 1000);
        req.version_ = version::http_1_1;
        req.headers_ = {{"host", "api.example.com"}};
        
        auto result = http1::encode_request(req, reused_buffer);
        ASSERT_TRUE(result.has_value());
    }, iterations);
    
    // 测试每次新分配缓冲区的性能
    auto avg_time_new = MeasureTime([&]() {
        request req;
        req.method_ = method::get;
        req.target_ = "/api/test/" + std::to_string(rand() % 1000);
        req.version_ = version::http_1_1;
        req.headers_ = {{"host", "api.example.com"}};
        
        auto result = http1::encode_request(req);
        ASSERT_TRUE(result.has_value());
    }, iterations);
    
    PrintBenchmarkResult("Buffer Reuse", avg_time_reused, iterations);
    PrintBenchmarkResult("New Buffer Allocation", avg_time_new, iterations);
    
    // 缓冲区重用应该更快
    std::cout << "Improvement: " << std::fixed << std::setprecision(1) 
              << (avg_time_new / avg_time_reused) << "x faster with buffer reuse" << std::endl;
}

// =============================================================================
// 压缩性能测试
// =============================================================================

TEST_F(BenchmarkTest, CompressionRatioBenchmark) {
    // 测试HPACK压缩比
    std::vector<header_field> typical_headers = {
        {":method", "GET"},
        {":scheme", "https"},
        {":path", "/api/users"},
        {":authority", "api.example.com"},
        {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"},
        {"accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8"},
        {"accept-language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"},
        {"accept-encoding", "gzip, deflate, br"},
        {"cache-control", "no-cache"},
        {"pragma", "no-cache"},
        {"sec-fetch-dest", "document"},
        {"sec-fetch-mode", "navigate"},
        {"sec-fetch-site", "none"},
        {"upgrade-insecure-requests", "1"}
    };
    
    // 计算原始大小
    size_t raw_size = 0;
    for (const auto& header : typical_headers) {
        raw_size += header.name.size() + header.value.size() + 4; // 加上": "和"\r\n"
    }
    
    // HPACK压缩
    detail::hpack_encoder encoder;
    output_buffer compressed_buffer;
    
    auto result = encoder.encode(typical_headers, compressed_buffer);
    ASSERT_TRUE(result.has_value());
    
    size_t compressed_size = compressed_buffer.size();
    double compression_ratio = static_cast<double>(raw_size) / compressed_size;
    
    std::cout << "\n=== HPACK Compression Ratio ===" << std::endl;
    std::cout << "Raw size: " << raw_size << " bytes" << std::endl;
    std::cout << "Compressed size: " << compressed_size << " bytes" << std::endl;
    std::cout << "Compression ratio: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "Space saved: " << std::fixed << std::setprecision(1) 
              << (1.0 - static_cast<double>(compressed_size) / raw_size) * 100 << "%" << std::endl;
    
    // HPACK应该提供良好的压缩比
    EXPECT_GT(compression_ratio, 2.0); // 至少2:1的压缩比
}

// =============================================================================
// 基准测试总结
// =============================================================================

TEST_F(BenchmarkTest, BenchmarkSummary) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "HTTP Parse Library Performance Summary" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "All benchmark tests completed." << std::endl;
    std::cout << "Check individual test results above for detailed performance metrics." << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    // 总是成功，这只是一个总结测试
    SUCCEED();
}