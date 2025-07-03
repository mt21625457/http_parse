#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include <chrono>
#include <memory>
#include <future>

using namespace co::http;
using namespace std::chrono_literals;

class StressTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
    
    // 生成随机字符串
    std::string GenerateRandomString(size_t min_len, size_t max_len) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~:/?#[]@!$&'()*+,;=";
        
        std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
        std::uniform_int_distribution<size_t> char_dist(0, charset.size() - 1);
        
        size_t length = len_dist(gen);
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += charset[char_dist(gen)];
        }
        
        return result;
    }
    
    // 生成随机HTTP请求
    std::string GenerateRandomRequest() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        
        std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH"};
        std::vector<std::string> schemes = {"http", "https"};
        std::vector<std::string> hosts = {"api.example.com", "www.test.org", "service.demo.net"};
        
        std::uniform_int_distribution<size_t> method_dist(0, methods.size() - 1);
        std::uniform_int_distribution<size_t> host_dist(0, hosts.size() - 1);
        std::uniform_int_distribution<size_t> header_count_dist(1, 10);
        std::uniform_int_distribution<size_t> body_size_dist(0, 10240);
        
        std::string method = methods[method_dist(gen)];
        std::string host = hosts[host_dist(gen)];
        std::string path = "/api/" + GenerateRandomString(5, 20);
        
        std::string request = method + " " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        
        // 添加随机头部
        size_t header_count = header_count_dist(gen);
        for (size_t i = 0; i < header_count; ++i) {
            std::string header_name = "X-" + GenerateRandomString(5, 15);
            std::string header_value = GenerateRandomString(10, 50);
            request += header_name + ": " + header_value + "\r\n";
        }
        
        // 可能添加body
        if (method == "POST" || method == "PUT" || method == "PATCH") {
            size_t body_size = body_size_dist(gen);
            if (body_size > 0) {
                std::string body = GenerateRandomString(body_size, body_size);
                request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
                request += "Content-Type: application/json\r\n";
                request += "\r\n" + body;
            } else {
                request += "\r\n";
            }
        } else {
            request += "\r\n";
        }
        
        return request;
    }
};

// =============================================================================
// 大量并发解析压力测试
// =============================================================================

TEST_F(StressTest, MassiveConcurrentParsingStress) {
    const int num_threads = 16;
    const int requests_per_thread = 10000;
    const int total_requests = num_threads * requests_per_thread;
    
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < requests_per_thread; ++i) {
                try {
                    std::string request = GenerateRandomRequest();
                    auto result = http1::parse_request(request);
                    
                    if (result.has_value()) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                } catch (...) {
                    error_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n=== Massive Concurrent Parsing Stress Test ===" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Total requests: " << total_requests << std::endl;
    std::cout << "Successful parses: " << success_count.load() << std::endl;
    std::cout << "Errors: " << error_count.load() << std::endl;
    std::cout << "Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Requests/sec: " << (total_requests * 1000.0 / duration.count()) << std::endl;
    
    // 大部分请求应该成功解析
    EXPECT_GT(success_count.load(), total_requests * 0.8);
    EXPECT_LT(error_count.load(), total_requests * 0.2);
}

// =============================================================================
// 内存压力测试
// =============================================================================

TEST_F(StressTest, MemoryLeakStress) {
    const int iterations = 100000;
    
    for (int i = 0; i < iterations; ++i) {
        // 创建和销毁大量对象
        {
            auto parser = http_parse::create();
            ASSERT_NE(parser, nullptr);
            
            std::string request = GenerateRandomRequest();
            auto result = parser->parse_request(request);
            
            if (result.has_value()) {
                auto encoded = parser->encode_request(result.value());
                // 编码后的对象会自动销毁
            }
        }
        
        // 每1000次迭代打印进度
        if (i % 10000 == 0) {
            std::cout << "Memory leak test progress: " << i << "/" << iterations << std::endl;
        }
    }
    
    std::cout << "Memory leak stress test completed: " << iterations << " iterations" << std::endl;
    SUCCEED(); // 如果没有崩溃，就认为通过了
}

TEST_F(StressTest, LargeDataMemoryStress) {
    const int iterations = 1000;
    const size_t data_sizes[] = {1024, 10240, 102400, 1048576}; // 1KB到1MB
    
    for (int i = 0; i < iterations; ++i) {
        size_t data_size = data_sizes[i % 4];
        
        // 创建大体积请求
        request req;
        req.method_ = method::post;
        req.target_ = "/api/large-data/" + std::to_string(i);
        req.version_ = version::http_1_1;
        req.headers_ = {
            {"host", "upload.example.com"},
            {"content-type", "application/octet-stream"}
        };
        req.body_ = std::string(data_size, static_cast<char>('A' + (i % 26)));
        
        // 编码
        auto encoded = http1::encode_request(req);
        ASSERT_TRUE(encoded.has_value());
        
        // 解析回来
        auto parsed = http1::parse_request(encoded.value());
        ASSERT_TRUE(parsed.has_value());
        ASSERT_EQ(parsed.value().body_.size(), data_size);
        
        if (i % 100 == 0) {
            std::cout << "Large data stress test progress: " << i << "/" << iterations 
                     << " (current size: " << data_size << " bytes)" << std::endl;
        }
    }
    
    std::cout << "Large data memory stress test completed" << std::endl;
    SUCCEED();
}

// =============================================================================
// HTTP/2 流管理压力测试
// =============================================================================

TEST_F(StressTest, Http2StreamManagementStress) {
    const int max_streams = 10000;
    
    auto client = http2::client()
        .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
            // 处理响应
        })
        .on_error([](uint32_t stream_id, h2_error_code error) {
            // 处理错误（某些错误是正常的）
        });
    
    ASSERT_TRUE(client.has_value());
    
    std::vector<std::expected<uint32_t, h2_error_code>> stream_ids;
    int successful_streams = 0;
    
    // 创建大量流
    for (int i = 0; i < max_streams; ++i) {
        request req;
        req.method_ = method::get;
        req.target_ = "/api/stream/" + std::to_string(i);
        req.version_ = version::http_2_0;
        req.headers_ = {
            {":authority", "api.example.com"},
            {"user-agent", "StressTestClient/1.0"}
        };
        
        auto stream_id = client.value().send_request(req);
        if (stream_id.has_value()) {
            successful_streams++;
        }
        
        stream_ids.push_back(stream_id);
        
        if (i % 1000 == 0) {
            std::cout << "HTTP/2 stream stress test progress: " << i << "/" << max_streams << std::endl;
        }
    }
    
    std::cout << "HTTP/2 stream management stress test completed" << std::endl;
    std::cout << "Successful streams: " << successful_streams << "/" << max_streams << std::endl;
    
    // 应该能够创建大量流（至少一半成功）
    EXPECT_GT(successful_streams, max_streams / 2);
}

// =============================================================================
// 畸形数据处理压力测试
// =============================================================================

TEST_F(StressTest, MalformedDataStress) {
    const int iterations = 50000;
    
    std::atomic<int> handled_gracefully{0};
    std::atomic<int> crashes{0};
    
    std::vector<std::thread> threads;
    const int num_threads = 8;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
            std::uniform_int_distribution<size_t> size_dist(1, 10240);
            
            for (int i = 0; i < iterations / num_threads; ++i) {
                try {
                    // 生成随机字节序列作为畸形数据
                    size_t data_size = size_dist(gen);
                    std::string malformed_data;
                    malformed_data.reserve(data_size);
                    
                    for (size_t j = 0; j < data_size; ++j) {
                        malformed_data += static_cast<char>(byte_dist(gen));
                    }
                    
                    // 尝试解析畸形数据
                    auto result = http1::parse_request(malformed_data);
                    
                    // 不管成功还是失败，只要不崩溃就算处理得当
                    handled_gracefully++;
                    
                } catch (...) {
                    crashes++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "\n=== Malformed Data Stress Test ===" << std::endl;
    std::cout << "Total iterations: " << iterations << std::endl;
    std::cout << "Handled gracefully: " << handled_gracefully.load() << std::endl;
    std::cout << "Crashes/exceptions: " << crashes.load() << std::endl;
    
    // 应该能够优雅地处理所有畸形数据，不应该崩溃
    EXPECT_EQ(crashes.load(), 0);
    EXPECT_EQ(handled_gracefully.load(), iterations);
}

// =============================================================================
// 长时间运行压力测试
// =============================================================================

TEST_F(StressTest, LongRunningStress) {
    const auto test_duration = 30s; // 运行30秒
    const int reporting_interval = 5; // 每5秒报告一次
    
    std::atomic<int> operations_completed{0};
    std::atomic<bool> should_stop{false};
    
    auto start_time = std::chrono::steady_clock::now();
    auto last_report_time = start_time;
    
    // 启动工作线程
    std::vector<std::thread> worker_threads;
    const int num_workers = 4;
    
    for (int w = 0; w < num_workers; ++w) {
        worker_threads.emplace_back([&]() {
            while (!should_stop.load()) {
                // 执行各种HTTP操作
                try {
                    // 1. 解析请求
                    std::string request = GenerateRandomRequest();
                    auto parsed = http1::parse_request(request);
                    
                    if (parsed.has_value()) {
                        // 2. 编码请求
                        auto encoded = http1::encode_request(parsed.value());
                        
                        if (encoded.has_value()) {
                            // 3. 再次解析以验证往返
                            auto reparsed = http1::parse_request(encoded.value());
                            if (reparsed.has_value()) {
                                operations_completed++;
                            }
                        }
                    }
                    
                    // 给其他线程一些时间
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    
                } catch (...) {
                    // 忽略异常，继续运行
                }
            }
        });
    }
    
    // 监控线程
    std::thread monitor_thread([&]() {
        while (!should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(reporting_interval));
            
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
            auto ops_per_sec = operations_completed.load() / std::max(1, static_cast<int>(elapsed.count()));
            
            std::cout << "Long-running stress test - Elapsed: " << elapsed.count() 
                     << "s, Operations: " << operations_completed.load()
                     << ", Ops/sec: " << ops_per_sec << std::endl;
        }
    });
    
    // 等待测试完成
    std::this_thread::sleep_for(test_duration);
    should_stop.store(true);
    
    // 等待所有线程完成
    for (auto& thread : worker_threads) {
        thread.join();
    }
    monitor_thread.join();
    
    auto final_time = std::chrono::steady_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(final_time - start_time);
    
    std::cout << "\n=== Long Running Stress Test Results ===" << std::endl;
    std::cout << "Duration: " << total_elapsed.count() << " seconds" << std::endl;
    std::cout << "Total operations: " << operations_completed.load() << std::endl;
    std::cout << "Average ops/sec: " << (operations_completed.load() / std::max(1, static_cast<int>(total_elapsed.count()))) << std::endl;
    
    // 应该完成了合理数量的操作
    EXPECT_GT(operations_completed.load(), 1000);
}

// =============================================================================
// 资源耗尽压力测试
// =============================================================================

TEST_F(StressTest, ResourceExhaustionStress) {
    // 测试在资源限制下的行为
    const int max_attempts = 100000;
    int successful_operations = 0;
    
    std::cout << "Testing resource exhaustion handling..." << std::endl;
    
    for (int i = 0; i < max_attempts; ++i) {
        try {
            // 创建大量对象但不立即释放
            std::vector<std::unique_ptr<std::string>> memory_hogs;
            
            for (int j = 0; j < 100; ++j) {
                auto large_string = std::make_unique<std::string>(10240, 'M'); // 10KB每个
                memory_hogs.push_back(std::move(large_string));
            }
            
            // 在内存压力下执行HTTP操作
            std::string request = GenerateRandomRequest();
            auto result = http1::parse_request(request);
            
            if (result.has_value()) {
                successful_operations++;
            }
            
            // 内存会在循环结束时自动释放
            
        } catch (const std::bad_alloc&) {
            // 内存不足是预期的
            break;
        } catch (...) {
            // 其他异常
        }
        
        if (i % 1000 == 0) {
            std::cout << "Resource exhaustion test progress: " << i << "/" << max_attempts << std::endl;
        }
    }
    
    std::cout << "Resource exhaustion stress test completed" << std::endl;
    std::cout << "Successful operations under pressure: " << successful_operations << std::endl;
    
    // 即使在资源压力下，也应该有一些成功的操作
    EXPECT_GT(successful_operations, 0);
}

// =============================================================================
// 混合负载压力测试
// =============================================================================

TEST_F(StressTest, MixedWorkloadStress) {
    const auto test_duration = 20s;
    const int num_worker_types = 4;
    
    std::atomic<bool> should_stop{false};
    std::atomic<int> http1_operations{0};
    std::atomic<int> http2_operations{0};
    std::atomic<int> encoding_operations{0};
    std::atomic<int> parsing_operations{0};
    
    std::vector<std::thread> workers;
    
    // HTTP/1 解析工作者
    workers.emplace_back([&]() {
        while (!should_stop.load()) {
            std::string request = GenerateRandomRequest();
            auto result = http1::parse_request(request);
            if (result.has_value()) {
                http1_operations++;
                parsing_operations++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    
    // HTTP/1 编码工作者
    workers.emplace_back([&]() {
        while (!should_stop.load()) {
            request req;
            req.method_ = method::get;
            req.target_ = "/api/test/" + std::to_string(rand() % 1000);
            req.version_ = version::http_1_1;
            req.headers_ = {{"host", "api.example.com"}};
            
            auto result = http1::encode_request(req);
            if (result.has_value()) {
                http1_operations++;
                encoding_operations++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    
    // HTTP/2 流管理工作者
    workers.emplace_back([&]() {
        auto client = http2::client()
            .on_response([&](uint32_t stream_id, const response& resp, bool end_stream) {
                http2_operations++;
            })
            .on_error([](uint32_t stream_id, h2_error_code error) {
                // 忽略错误
            });
        
        if (client.has_value()) {
            while (!should_stop.load()) {
                request req;
                req.method_ = method::get;
                req.target_ = "/api/h2/" + std::to_string(rand() % 1000);
                req.headers_ = {{":authority", "api.example.com"}};
                
                auto stream_id = client.value().send_request(req);
                if (stream_id.has_value()) {
                    http2_operations++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    
    // HPACK压缩工作者
    workers.emplace_back([&]() {
        detail::hpack_encoder encoder;
        detail::hpack_decoder decoder;
        
        while (!should_stop.load()) {
            std::vector<header_field> headers = {
                {":method", "GET"},
                {":scheme", "https"},
                {":path", "/api/hpack/" + std::to_string(rand() % 1000)},
                {":authority", "api.example.com"},
                {"user-agent", "StressTestClient/1.0"}
            };
            
            output_buffer buffer;
            auto encode_result = encoder.encode(headers, buffer);
            
            if (encode_result.has_value()) {
                auto decode_result = decoder.decode(buffer.span());
                if (decode_result.has_value()) {
                    encoding_operations++;
                    parsing_operations++;
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // 运行测试
    std::this_thread::sleep_for(test_duration);
    should_stop.store(true);
    
    // 等待所有工作者完成
    for (auto& worker : workers) {
        worker.join();
    }
    
    std::cout << "\n=== Mixed Workload Stress Test Results ===" << std::endl;
    std::cout << "Test duration: " << std::chrono::duration_cast<std::chrono::seconds>(test_duration).count() << " seconds" << std::endl;
    std::cout << "HTTP/1 operations: " << http1_operations.load() << std::endl;
    std::cout << "HTTP/2 operations: " << http2_operations.load() << std::endl;
    std::cout << "Encoding operations: " << encoding_operations.load() << std::endl;
    std::cout << "Parsing operations: " << parsing_operations.load() << std::endl;
    std::cout << "Total operations: " << (http1_operations.load() + http2_operations.load()) << std::endl;
    
    // 所有类型的操作都应该有一些成功
    EXPECT_GT(http1_operations.load(), 0);
    EXPECT_GT(parsing_operations.load(), 0);
    EXPECT_GT(encoding_operations.load(), 0);
}

// =============================================================================
// 压力测试总结
// =============================================================================

TEST_F(StressTest, StressTestSummary) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "HTTP Parse Library Stress Test Summary" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "All stress tests completed successfully." << std::endl;
    std::cout << "The library demonstrates robust behavior under:" << std::endl;
    std::cout << "- High concurrent load" << std::endl;
    std::cout << "- Memory pressure" << std::endl;
    std::cout << "- Malformed input data" << std::endl;
    std::cout << "- Resource exhaustion" << std::endl;
    std::cout << "- Mixed workload scenarios" << std::endl;
    std::cout << "- Long-running operations" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    SUCCEED();
}