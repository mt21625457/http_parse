#include <gtest/gtest.h>
#include "http_parse.hpp"
#include "http_parse/v2/frame_processor.hpp"
#include "http_parse/v2/types.hpp"
#include <string>
#include <vector>
#include <array>

using namespace co::http::v2;

class Http2FrameProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
        processor = std::make_unique<frame_processor>();
        received_frames.clear();
        received_data.clear();
        received_errors.clear();
    }
    
    void TearDown() override {
        // 每个测试后的清理
        processor.reset();
    }
    
    // 回调函数用于测试
    void OnFrame(const frame_header& header, std::span<const uint8_t> payload) {
        received_frames.push_back({header, std::vector<uint8_t>(payload.begin(), payload.end())});
    }
    
    void OnData(uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        received_data.push_back({stream_id, std::vector<uint8_t>(data.begin(), data.end()), end_stream});
    }
    
    void OnError(h2_error_code error, const std::string& description) {
        received_errors.push_back({error, description});
    }
    
    std::unique_ptr<frame_processor> processor;
    
    struct FrameData {
        frame_header header;
        std::vector<uint8_t> payload;
    };
    
    struct DataInfo {
        uint32_t stream_id;
        std::vector<uint8_t> data;
        bool end_stream;
    };
    
    struct ErrorInfo {
        h2_error_code error;
        std::string description;
    };
    
    std::vector<FrameData> received_frames;
    std::vector<DataInfo> received_data;
    std::vector<ErrorInfo> received_errors;
    
    // 辅助函数：构造帧数据
    std::vector<uint8_t> CreateFrame(frame_type type, uint8_t flags, uint32_t stream_id, 
                                   const std::vector<uint8_t>& payload = {}) {
        std::vector<uint8_t> frame(9 + payload.size());
        
        // 帧头
        frame[0] = (payload.size() >> 16) & 0xFF;
        frame[1] = (payload.size() >> 8) & 0xFF;
        frame[2] = payload.size() & 0xFF;
        frame[3] = static_cast<uint8_t>(type);
        frame[4] = flags;
        frame[5] = (stream_id >> 24) & 0x7F; // 清除保留位
        frame[6] = (stream_id >> 16) & 0xFF;
        frame[7] = (stream_id >> 8) & 0xFF;
        frame[8] = stream_id & 0xFF;
        
        // 载荷
        if (!payload.empty()) {
            std::copy(payload.begin(), payload.end(), frame.begin() + 9);
        }
        
        return frame;
    }
};

// =============================================================================
// HTTP/2 帧处理器基本功能测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, ProcessDataFrame) {
    // 设置回调
    processor->set_data_callback([this](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        OnData(stream_id, data, end_stream);
    });
    
    // 创建DATA帧
    std::string test_data = "Hello, HTTP/2!";
    std::vector<uint8_t> payload(test_data.begin(), test_data.end());
    auto frame = CreateFrame(frame_type::data, 
                           static_cast<uint8_t>(frame_flags::end_stream), 
                           1, payload);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), frame.size());
    
    // 验证回调
    ASSERT_EQ(received_data.size(), 1);
    EXPECT_EQ(received_data[0].stream_id, 1);
    EXPECT_EQ(received_data[0].data, payload);
    EXPECT_TRUE(received_data[0].end_stream);
}

TEST_F(Http2FrameProcessorTest, ProcessHeadersFrame) {
    // 设置回调
    processor->set_headers_callback([this](uint32_t stream_id, const std::vector<header_field>& headers, bool end_stream) {
        // 测试用简单验证
        EXPECT_EQ(stream_id, 1);
        EXPECT_TRUE(end_stream);
    });
    
    // 创建HEADERS帧（简化的头部数据）
    std::vector<uint8_t> headers_data = {
        0x82, // :method: GET (indexed)
        0x86, // :scheme: https (indexed)
        0x84, // :path: / (indexed)
        0x01, 0x0f, // :authority: (literal with incremental indexing)
        'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'
    };
    
    auto frame = CreateFrame(frame_type::headers, 
                           static_cast<uint8_t>(frame_flags::end_headers | frame_flags::end_stream), 
                           1, headers_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), frame.size());
}

TEST_F(Http2FrameProcessorTest, ProcessSettingsFrame) {
    // 设置回调
    processor->set_settings_callback([this](const std::vector<setting>& settings) {
        EXPECT_EQ(settings.size(), 2);
        EXPECT_EQ(settings[0].id, static_cast<uint16_t>(settings_id::header_table_size));
        EXPECT_EQ(settings[0].value, 8192);
        EXPECT_EQ(settings[1].id, static_cast<uint16_t>(settings_id::max_concurrent_streams));
        EXPECT_EQ(settings[1].value, 100);
    });
    
    // 创建SETTINGS帧
    std::vector<uint8_t> settings_data = {
        0x00, 0x01, 0x00, 0x00, 0x20, 0x00, // HEADER_TABLE_SIZE = 8192
        0x00, 0x03, 0x00, 0x00, 0x00, 0x64  // MAX_CONCURRENT_STREAMS = 100
    };
    
    auto frame = CreateFrame(frame_type::settings, 0, 0, settings_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), frame.size());
}

TEST_F(Http2FrameProcessorTest, ProcessPingFrame) {
    // 设置回调
    processor->set_ping_callback([this](const std::array<uint8_t, 8>& data, bool ack) {
        EXPECT_FALSE(ack);
        EXPECT_EQ(data[0], 0x01);
        EXPECT_EQ(data[7], 0x08);
    });
    
    // 创建PING帧
    std::vector<uint8_t> ping_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto frame = CreateFrame(frame_type::ping, 0, 0, ping_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), frame.size());
}

TEST_F(Http2FrameProcessorTest, ProcessGoAwayFrame) {
    // 设置回调
    processor->set_goaway_callback([this](uint32_t last_stream_id, h2_error_code error_code, std::span<const uint8_t> debug_data) {
        EXPECT_EQ(last_stream_id, 7);
        EXPECT_EQ(error_code, h2_error_code::protocol_error);
        std::string debug_str(debug_data.begin(), debug_data.end());
        EXPECT_EQ(debug_str, "test");
    });
    
    // 创建GOAWAY帧
    std::vector<uint8_t> goaway_data = {
        0x00, 0x00, 0x00, 0x07, // Last Stream ID = 7
        0x00, 0x00, 0x00, 0x01, // Error Code = PROTOCOL_ERROR
        't', 'e', 's', 't'      // Debug Data
    };
    
    auto frame = CreateFrame(frame_type::goaway, 0, 0, goaway_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), frame.size());
}

// =============================================================================
// HTTP/2 帧验证测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, InvalidFrameSize) {
    // 创建过大的帧
    std::vector<uint8_t> large_payload(100000); // 100KB
    auto frame = CreateFrame(frame_type::data, 0, 1, large_payload);
    
    // 设置错误回调
    processor->set_error_callback([this](h2_error_code error, const std::string& description) {
        OnError(error, description);
    });
    
    // 处理帧应该失败
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_FALSE(result.has_value());
    
    // 应该收到错误
    ASSERT_EQ(received_errors.size(), 1);
    EXPECT_EQ(received_errors[0].error, h2_error_code::frame_size_error);
}

TEST_F(Http2FrameProcessorTest, InvalidStreamId) {
    // 设置错误回调
    processor->set_error_callback([this](h2_error_code error, const std::string& description) {
        OnError(error, description);
    });
    
    // DATA帧不能使用流ID 0
    auto frame = CreateFrame(frame_type::data, 0, 0, {0x01, 0x02, 0x03});
    
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_FALSE(result.has_value());
    
    // 应该收到协议错误
    ASSERT_EQ(received_errors.size(), 1);
    EXPECT_EQ(received_errors[0].error, h2_error_code::protocol_error);
}

TEST_F(Http2FrameProcessorTest, InvalidSettingsFrame) {
    // 设置错误回调
    processor->set_error_callback([this](h2_error_code error, const std::string& description) {
        OnError(error, description);
    });
    
    // SETTINGS帧长度不是6的倍数
    std::vector<uint8_t> invalid_settings = {0x00, 0x01, 0x00, 0x00, 0x20}; // 缺少一个字节
    auto frame = CreateFrame(frame_type::settings, 0, 0, invalid_settings);
    
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_FALSE(result.has_value());
    
    // 应该收到帧大小错误
    ASSERT_EQ(received_errors.size(), 1);
    EXPECT_EQ(received_errors[0].error, h2_error_code::frame_size_error);
}

TEST_F(Http2FrameProcessorTest, InvalidPingFrame) {
    // 设置错误回调
    processor->set_error_callback([this](h2_error_code error, const std::string& description) {
        OnError(error, description);
    });
    
    // PING帧长度必须是8字节
    std::vector<uint8_t> invalid_ping = {0x01, 0x02, 0x03, 0x04}; // 只有4字节
    auto frame = CreateFrame(frame_type::ping, 0, 0, invalid_ping);
    
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_FALSE(result.has_value());
    
    // 应该收到帧大小错误
    ASSERT_EQ(received_errors.size(), 1);
    EXPECT_EQ(received_errors[0].error, h2_error_code::frame_size_error);
}

// =============================================================================
// HTTP/2 增量处理测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, IncrementalFrameProcessing) {
    // 设置回调
    processor->set_data_callback([this](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        OnData(stream_id, data, end_stream);
    });
    
    // 创建一个完整的帧
    std::string test_data = "Hello, HTTP/2!";
    std::vector<uint8_t> payload(test_data.begin(), test_data.end());
    auto full_frame = CreateFrame(frame_type::data, 
                                static_cast<uint8_t>(frame_flags::end_stream), 
                                1, payload);
    
    // 分块处理
    const size_t chunk_size = 4;
    size_t processed = 0;
    
    for (size_t i = 0; i < full_frame.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, full_frame.size());
        std::span<const uint8_t> chunk(full_frame.data() + i, end - i);
        
        auto result = processor->process_frame(chunk);
        if (result.has_value()) {
            processed += result.value();
        }
        
        // 如果处理完成，应该收到回调
        if (processed == full_frame.size()) {
            break;
        }
    }
    
    // 验证处理完成
    EXPECT_EQ(processed, full_frame.size());
    ASSERT_EQ(received_data.size(), 1);
    EXPECT_EQ(received_data[0].stream_id, 1);
    EXPECT_EQ(received_data[0].data, payload);
}

TEST_F(Http2FrameProcessorTest, MultipleFramesInBuffer) {
    // 设置回调
    processor->set_data_callback([this](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        OnData(stream_id, data, end_stream);
    });
    
    // 创建多个帧
    std::vector<uint8_t> buffer;
    
    // 第一个帧
    std::string data1 = "Frame 1";
    std::vector<uint8_t> payload1(data1.begin(), data1.end());
    auto frame1 = CreateFrame(frame_type::data, 0, 1, payload1);
    buffer.insert(buffer.end(), frame1.begin(), frame1.end());
    
    // 第二个帧
    std::string data2 = "Frame 2";
    std::vector<uint8_t> payload2(data2.begin(), data2.end());
    auto frame2 = CreateFrame(frame_type::data, 
                            static_cast<uint8_t>(frame_flags::end_stream), 
                            3, payload2);
    buffer.insert(buffer.end(), frame2.begin(), frame2.end());
    
    // 处理整个缓冲区
    auto result = processor->process_frame(std::span<const uint8_t>(buffer));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), buffer.size());
    
    // 应该收到两个帧的回调
    ASSERT_EQ(received_data.size(), 2);
    EXPECT_EQ(received_data[0].stream_id, 1);
    EXPECT_EQ(received_data[0].data, payload1);
    EXPECT_FALSE(received_data[0].end_stream);
    EXPECT_EQ(received_data[1].stream_id, 3);
    EXPECT_EQ(received_data[1].data, payload2);
    EXPECT_TRUE(received_data[1].end_stream);
}

// =============================================================================
// HTTP/2 状态管理测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, SettingsAcknowledgment) {
    bool settings_ack_received = false;
    
    // 设置回调
    processor->set_settings_callback([&settings_ack_received](const std::vector<setting>& settings) {
        if (settings.empty()) {
            settings_ack_received = true; // ACK帧没有设置
        }
    });
    
    // 创建SETTINGS ACK帧
    auto frame = CreateFrame(frame_type::settings, 
                           static_cast<uint8_t>(frame_flags::ack), 
                           0, {});
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(settings_ack_received);
}

TEST_F(Http2FrameProcessorTest, PingAcknowledgment) {
    bool ping_ack_received = false;
    
    // 设置回调
    processor->set_ping_callback([&ping_ack_received](const std::array<uint8_t, 8>& data, bool ack) {
        if (ack) {
            ping_ack_received = true;
        }
    });
    
    // 创建PING ACK帧
    std::vector<uint8_t> ping_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto frame = CreateFrame(frame_type::ping, 
                           static_cast<uint8_t>(frame_flags::ack), 
                           0, ping_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(ping_ack_received);
}

// =============================================================================
// HTTP/2 流控制测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, WindowUpdateFrame) {
    uint32_t received_increment = 0;
    uint32_t received_stream_id = 0;
    
    // 设置回调
    processor->set_window_update_callback([&](uint32_t stream_id, uint32_t window_size_increment) {
        received_stream_id = stream_id;
        received_increment = window_size_increment;
    });
    
    // 创建WINDOW_UPDATE帧
    std::vector<uint8_t> window_data = {
        0x00, 0x00, 0x10, 0x00 // Window Size Increment = 4096
    };
    
    auto frame = CreateFrame(frame_type::window_update, 0, 1, window_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(received_stream_id, 1);
    EXPECT_EQ(received_increment, 4096);
}

TEST_F(Http2FrameProcessorTest, RstStreamFrame) {
    uint32_t received_stream_id = 0;
    h2_error_code received_error = h2_error_code::no_error;
    
    // 设置回调
    processor->set_rst_stream_callback([&](uint32_t stream_id, h2_error_code error_code) {
        received_stream_id = stream_id;
        received_error = error_code;
    });
    
    // 创建RST_STREAM帧
    std::vector<uint8_t> rst_data = {
        0x00, 0x00, 0x00, 0x08 // Error Code = CANCEL
    };
    
    auto frame = CreateFrame(frame_type::rst_stream, 0, 5, rst_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(received_stream_id, 5);
    EXPECT_EQ(received_error, h2_error_code::cancel);
}

// =============================================================================
// HTTP/2 帧优先级测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, PriorityFrame) {
    uint32_t received_stream_id = 0;
    uint32_t received_dependency = 0;
    uint8_t received_weight = 0;
    bool received_exclusive = false;
    
    // 设置回调
    processor->set_priority_callback([&](uint32_t stream_id, uint32_t stream_dependency, uint8_t weight, bool exclusive) {
        received_stream_id = stream_id;
        received_dependency = stream_dependency;
        received_weight = weight;
        received_exclusive = exclusive;
    });
    
    // 创建PRIORITY帧
    std::vector<uint8_t> priority_data = {
        0x80, 0x00, 0x00, 0x03, // Stream Dependency = 3 (exclusive)
        0x64                     // Weight = 100
    };
    
    auto frame = CreateFrame(frame_type::priority, 0, 1, priority_data);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(received_stream_id, 1);
    EXPECT_EQ(received_dependency, 3);
    EXPECT_EQ(received_weight, 100);
    EXPECT_TRUE(received_exclusive);
}

// =============================================================================
// HTTP/2 性能测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, HighVolumeFrameProcessing) {
    size_t frame_count = 0;
    
    // 设置回调
    processor->set_data_callback([&frame_count](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        frame_count++;
    });
    
    // 创建1000个DATA帧
    std::vector<uint8_t> buffer;
    for (int i = 1; i <= 1000; i += 2) {
        std::string data = "Frame " + std::to_string(i);
        std::vector<uint8_t> payload(data.begin(), data.end());
        auto frame = CreateFrame(frame_type::data, 0, i, payload);
        buffer.insert(buffer.end(), frame.begin(), frame.end());
    }
    
    // 处理所有帧
    auto result = processor->process_frame(std::span<const uint8_t>(buffer));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), buffer.size());
    EXPECT_EQ(frame_count, 500); // 500个帧
}

TEST_F(Http2FrameProcessorTest, LargeFrameProcessing) {
    bool large_frame_received = false;
    
    // 设置回调
    processor->set_data_callback([&large_frame_received](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        if (data.size() == 16384) { // 16KB
            large_frame_received = true;
        }
    });
    
    // 创建大帧（16KB）
    std::vector<uint8_t> large_payload(16384, 'A');
    auto frame = CreateFrame(frame_type::data, 0, 1, large_payload);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), frame.size());
    EXPECT_TRUE(large_frame_received);
}

// =============================================================================
// HTTP/2 错误恢复测试
// =============================================================================

TEST_F(Http2FrameProcessorTest, ProcessorReset) {
    // 设置回调
    processor->set_data_callback([this](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        OnData(stream_id, data, end_stream);
    });
    
    // 处理一个帧
    std::string data = "Test data";
    std::vector<uint8_t> payload(data.begin(), data.end());
    auto frame = CreateFrame(frame_type::data, 0, 1, payload);
    
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(received_data.size(), 1);
    
    // 重置处理器
    processor->reset();
    
    // 重置后应该能继续处理帧
    received_data.clear();
    result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(received_data.size(), 1);
}

TEST_F(Http2FrameProcessorTest, UnknownFrameType) {
    // 设置回调捕获未知帧
    processor->set_unknown_frame_callback([this](const frame_header& header, std::span<const uint8_t> payload) {
        OnFrame(header, payload);
    });
    
    // 创建未知类型的帧
    std::vector<uint8_t> unknown_payload = {0x01, 0x02, 0x03};
    auto frame = CreateFrame(static_cast<frame_type>(0xFF), 0, 1, unknown_payload);
    
    // 处理帧
    auto result = processor->process_frame(std::span<const uint8_t>(frame));
    EXPECT_TRUE(result.has_value());
    
    // 应该收到未知帧回调
    ASSERT_EQ(received_frames.size(), 1);
    EXPECT_EQ(received_frames[0].header.type, 0xFF);
    EXPECT_EQ(received_frames[0].header.stream_id, 1);
    EXPECT_EQ(received_frames[0].payload, unknown_payload);
}