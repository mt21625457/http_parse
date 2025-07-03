#include <gtest/gtest.h>
#include "http_parse.hpp"
#include "http_parse/v2/types.hpp"
#include <string>
#include <array>

using namespace co::http::v2;

class Http2TypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
};

// =============================================================================
// HTTP/2 枚举类型测试
// =============================================================================

TEST_F(Http2TypesTest, FrameTypeValues) {
    EXPECT_EQ(static_cast<uint8_t>(frame_type::data), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::headers), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::priority), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::rst_stream), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::settings), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::push_promise), 0x05);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::ping), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::goaway), 0x07);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::window_update), 0x08);
    EXPECT_EQ(static_cast<uint8_t>(frame_type::continuation), 0x09);
}

TEST_F(Http2TypesTest, FrameFlagsValues) {
    EXPECT_EQ(static_cast<uint8_t>(frame_flags::none), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(frame_flags::end_stream), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(frame_flags::ack), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(frame_flags::end_headers), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(frame_flags::padded), 0x08);
    EXPECT_EQ(static_cast<uint8_t>(frame_flags::priority_flag), 0x20);
}

TEST_F(Http2TypesTest, ErrorCodeValues) {
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::no_error), 0x00);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::protocol_error), 0x01);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::internal_error), 0x02);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::flow_control_error), 0x03);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::settings_timeout), 0x04);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::stream_closed), 0x05);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::frame_size_error), 0x06);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::refused_stream), 0x07);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::cancel), 0x08);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::compression_error), 0x09);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::connect_error), 0x0a);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::enhance_your_calm), 0x0b);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::inadequate_security), 0x0c);
    EXPECT_EQ(static_cast<uint32_t>(h2_error_code::http_1_1_required), 0x0d);
}

TEST_F(Http2TypesTest, SettingsIdValues) {
    EXPECT_EQ(static_cast<uint16_t>(settings_id::header_table_size), 0x01);
    EXPECT_EQ(static_cast<uint16_t>(settings_id::enable_push), 0x02);
    EXPECT_EQ(static_cast<uint16_t>(settings_id::max_concurrent_streams), 0x03);
    EXPECT_EQ(static_cast<uint16_t>(settings_id::initial_window_size), 0x04);
    EXPECT_EQ(static_cast<uint16_t>(settings_id::max_frame_size), 0x05);
    EXPECT_EQ(static_cast<uint16_t>(settings_id::max_header_list_size), 0x06);
}

// =============================================================================
// HTTP/2 帧头结构测试
// =============================================================================

TEST_F(Http2TypesTest, FrameHeaderSize) {
    EXPECT_EQ(frame_header::size, 9);
}

TEST_F(Http2TypesTest, FrameHeaderParsing) {
    // 构造一个标准的HTTP/2帧头
    std::array<uint8_t, 9> header_bytes = {
        0x00, 0x00, 0x08,  // Length: 8
        0x01,              // Type: HEADERS
        0x05,              // Flags: END_STREAM | END_HEADERS  
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1
    };
    
    auto header = frame_header::parse(header_bytes.data());
    
    EXPECT_EQ(header.length, 8);
    EXPECT_EQ(header.type, 0x01);
    EXPECT_EQ(header.flags, 0x05);
    EXPECT_EQ(header.stream_id, 1);
}

TEST_F(Http2TypesTest, FrameHeaderSerialization) {
    frame_header header;
    header.length = 1024;
    header.type = static_cast<uint8_t>(frame_type::data);
    header.flags = static_cast<uint8_t>(frame_flags::end_stream);
    header.stream_id = 42;
    
    std::array<uint8_t, 9> serialized;
    header.serialize(serialized.data());
    
    // 重新解析验证
    auto parsed = frame_header::parse(serialized.data());
    
    EXPECT_EQ(parsed.length, header.length);
    EXPECT_EQ(parsed.type, header.type);
    EXPECT_EQ(parsed.flags, header.flags);
    EXPECT_EQ(parsed.stream_id, header.stream_id);
}

TEST_F(Http2TypesTest, FrameHeaderMaxValues) {
    frame_header header;
    header.length = (1 << 24) - 1;  // 最大24位值
    header.type = 0xFF;
    header.flags = 0xFF;
    header.stream_id = (1U << 31) - 1;  // 最大31位值
    
    std::array<uint8_t, 9> serialized;
    header.serialize(serialized.data());
    
    auto parsed = frame_header::parse(serialized.data());
    
    EXPECT_EQ(parsed.length, header.length);
    EXPECT_EQ(parsed.type, header.type);
    EXPECT_EQ(parsed.flags, header.flags);
    EXPECT_EQ(parsed.stream_id, header.stream_id);
}

TEST_F(Http2TypesTest, FrameHeaderReservedBit) {
    // 测试保留位被正确清除
    std::array<uint8_t, 9> header_bytes = {
        0x00, 0x00, 0x00,  // Length: 0
        0x00,              // Type: DATA
        0x00,              // Flags: none
        0x80, 0x00, 0x00, 0x01  // Stream ID: 1 with reserved bit set
    };
    
    auto header = frame_header::parse(header_bytes.data());
    EXPECT_EQ(header.stream_id, 1);  // 保留位应该被清除
    
    // 序列化时保留位应该保持清除
    std::array<uint8_t, 9> serialized;
    header.serialize(serialized.data());
    EXPECT_EQ(serialized[5] & 0x80, 0);  // 保留位应该是0
}

// =============================================================================
// HTTP/2 设置帧测试
// =============================================================================

TEST_F(Http2TypesTest, SettingsFrameDefaults) {
    settings_frame settings;
    
    EXPECT_EQ(settings_frame::default_header_table_size, 4096);
    EXPECT_EQ(settings_frame::default_enable_push, 1);
    EXPECT_EQ(settings_frame::default_max_concurrent_streams, UINT32_MAX);
    EXPECT_EQ(settings_frame::default_initial_window_size, 65535);
    EXPECT_EQ(settings_frame::default_max_frame_size, 16384);
    EXPECT_EQ(settings_frame::default_max_header_list_size, UINT32_MAX);
    
    EXPECT_FALSE(settings.ack);
    EXPECT_TRUE(settings.settings.empty());
}

// =============================================================================
// HTTP/2 协议限制测试
// =============================================================================

TEST_F(Http2TypesTest, ProtocolLimits) {
    EXPECT_EQ(protocol_limits::max_frame_size_limit, (1u << 24) - 1);
    EXPECT_EQ(protocol_limits::min_max_frame_size, 16384);
    EXPECT_EQ(protocol_limits::max_window_size, (1u << 31) - 1);
    EXPECT_EQ(protocol_limits::max_stream_id, (1u << 31) - 1);
    EXPECT_EQ(protocol_limits::max_header_list_size_limit, UINT32_MAX);
}

// =============================================================================
// HTTP/2 流信息测试
// =============================================================================

TEST_F(Http2TypesTest, StreamInfoDefaults) {
    stream_info stream;
    
    EXPECT_EQ(stream.id, 0);
    EXPECT_EQ(stream.state, stream_state::idle);
    EXPECT_EQ(stream.window_size, settings_frame::default_initial_window_size);
    EXPECT_EQ(stream.remote_window_size, settings_frame::default_initial_window_size);
    EXPECT_EQ(stream.dependency, 0);
    EXPECT_EQ(stream.weight, 16);
    EXPECT_FALSE(stream.exclusive);
    EXPECT_FALSE(stream.headers_complete);
    EXPECT_FALSE(stream.data_complete);
    EXPECT_FALSE(stream.local_closed);
    EXPECT_FALSE(stream.remote_closed);
    EXPECT_EQ(stream.error, h2_error_code::no_error);
}

TEST_F(Http2TypesTest, StreamInfoStateMethods) {
    stream_info stream;
    
    // 测试初始状态
    EXPECT_FALSE(stream.is_closed());
    EXPECT_FALSE(stream.is_half_closed_local());
    EXPECT_FALSE(stream.is_half_closed_remote());
    EXPECT_TRUE(stream.can_send_data());
    EXPECT_TRUE(stream.can_receive_data());
    
    // 测试关闭状态
    stream.state = stream_state::closed;
    EXPECT_TRUE(stream.is_closed());
    EXPECT_FALSE(stream.can_send_data());
    EXPECT_FALSE(stream.can_receive_data());
    
    // 测试半关闭状态
    stream.state = stream_state::half_closed_local;
    EXPECT_FALSE(stream.is_closed());
    EXPECT_TRUE(stream.is_half_closed_local());
    EXPECT_FALSE(stream.is_half_closed_remote());
    EXPECT_FALSE(stream.can_send_data());
    EXPECT_TRUE(stream.can_receive_data());
    
    stream.state = stream_state::half_closed_remote;
    EXPECT_FALSE(stream.is_closed());
    EXPECT_FALSE(stream.is_half_closed_local());
    EXPECT_TRUE(stream.is_half_closed_remote());
    EXPECT_TRUE(stream.can_send_data());
    EXPECT_FALSE(stream.can_receive_data());
    
    // 测试窗口大小影响
    stream.state = stream_state::open;
    stream.window_size = 0;
    EXPECT_FALSE(stream.can_send_data());  // 窗口为0时不能发送
}

// =============================================================================
// HTTP/2 连接设置测试
// =============================================================================

TEST_F(Http2TypesTest, ConnectionSettingsDefaults) {
    connection_settings settings;
    
    EXPECT_EQ(settings.header_table_size, settings_frame::default_header_table_size);
    EXPECT_EQ(settings.enable_push, static_cast<bool>(settings_frame::default_enable_push));
    EXPECT_EQ(settings.max_concurrent_streams, settings_frame::default_max_concurrent_streams);
    EXPECT_EQ(settings.initial_window_size, settings_frame::default_initial_window_size);
    EXPECT_EQ(settings.max_frame_size, settings_frame::default_max_frame_size);
    EXPECT_EQ(settings.max_header_list_size, settings_frame::default_max_header_list_size);
    EXPECT_EQ(settings.connection_window_size, settings_frame::default_initial_window_size);
    EXPECT_EQ(settings.remote_connection_window_size, settings_frame::default_initial_window_size);
}

TEST_F(Http2TypesTest, ConnectionSettingsApplication) {
    connection_settings settings;
    
    // 测试header_table_size设置
    settings.apply_setting(static_cast<uint16_t>(settings_id::header_table_size), 8192);
    EXPECT_EQ(settings.header_table_size, 8192);
    
    // 测试enable_push设置
    settings.apply_setting(static_cast<uint16_t>(settings_id::enable_push), 0);
    EXPECT_FALSE(settings.enable_push);
    settings.apply_setting(static_cast<uint16_t>(settings_id::enable_push), 1);
    EXPECT_TRUE(settings.enable_push);
    
    // 测试max_concurrent_streams设置
    settings.apply_setting(static_cast<uint16_t>(settings_id::max_concurrent_streams), 100);
    EXPECT_EQ(settings.max_concurrent_streams, 100);
    
    // 测试initial_window_size设置
    settings.apply_setting(static_cast<uint16_t>(settings_id::initial_window_size), 32768);
    EXPECT_EQ(settings.initial_window_size, 32768);
    
    // 测试max_frame_size设置
    settings.apply_setting(static_cast<uint16_t>(settings_id::max_frame_size), 32768);
    EXPECT_EQ(settings.max_frame_size, 32768);
    
    // 测试max_header_list_size设置
    settings.apply_setting(static_cast<uint16_t>(settings_id::max_header_list_size), 16384);
    EXPECT_EQ(settings.max_header_list_size, 16384);
}

TEST_F(Http2TypesTest, ConnectionSettingsValidation) {
    connection_settings settings;
    
    // 测试有效设置
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::header_table_size), 8192));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::enable_push), 0));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::enable_push), 1));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::max_concurrent_streams), 100));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::initial_window_size), 65535));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::max_frame_size), 16384));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::max_frame_size), 32768));
    EXPECT_TRUE(settings.validate_setting(static_cast<uint16_t>(settings_id::max_header_list_size), 8192));
    
    // 测试无效设置
    EXPECT_FALSE(settings.validate_setting(static_cast<uint16_t>(settings_id::enable_push), 2));  // 必须是0或1
    EXPECT_FALSE(settings.validate_setting(static_cast<uint16_t>(settings_id::initial_window_size), 
                                          protocol_limits::max_window_size + 1));  // 超出最大窗口
    EXPECT_FALSE(settings.validate_setting(static_cast<uint16_t>(settings_id::max_frame_size), 8192));  // 小于最小值
    EXPECT_FALSE(settings.validate_setting(static_cast<uint16_t>(settings_id::max_frame_size), 
                                          protocol_limits::max_frame_size_limit + 1));  // 超出最大值
    
    // 测试未知设置ID（应该被接受）
    EXPECT_TRUE(settings.validate_setting(0xFF, 12345));
}

// =============================================================================
// HTTP/2 连接前置测试
// =============================================================================

TEST_F(Http2TypesTest, ConnectionPreface) {
    EXPECT_EQ(connection_preface, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
    EXPECT_EQ(connection_preface.size(), 24);
}

// =============================================================================
// 优先级和窗口更新帧测试
// =============================================================================

TEST_F(Http2TypesTest, PriorityFrame) {
    priority_frame frame;
    frame.stream_dependency = 42;
    frame.weight = 200;
    frame.exclusive = true;
    
    EXPECT_EQ(frame.stream_dependency, 42);
    EXPECT_EQ(frame.weight, 200);
    EXPECT_TRUE(frame.exclusive);
}

TEST_F(Http2TypesTest, WindowUpdateFrame) {
    window_update_frame frame;
    frame.window_size_increment = 32768;
    
    EXPECT_EQ(frame.window_size_increment, 32768);
}

// =============================================================================
// 边界条件和错误情况测试
// =============================================================================

TEST_F(Http2TypesTest, MaxStreamId) {
    stream_info stream;
    stream.id = protocol_limits::max_stream_id;
    
    EXPECT_EQ(stream.id, (1U << 31) - 1);
    EXPECT_GT(stream.id, 0);  // 确保是正数
}

TEST_F(Http2TypesTest, StreamStateTransitions) {
    stream_info stream;
    
    // idle -> open
    stream.state = stream_state::idle;
    stream.state = stream_state::open;
    EXPECT_EQ(stream.state, stream_state::open);
    
    // open -> half_closed_local
    stream.state = stream_state::half_closed_local;
    EXPECT_TRUE(stream.is_half_closed_local());
    EXPECT_FALSE(stream.can_send_data());
    
    // open -> half_closed_remote
    stream.state = stream_state::open;
    stream.state = stream_state::half_closed_remote;
    EXPECT_TRUE(stream.is_half_closed_remote());
    EXPECT_FALSE(stream.can_receive_data());
    
    // any -> closed
    stream.state = stream_state::closed;
    EXPECT_TRUE(stream.is_closed());
    EXPECT_FALSE(stream.can_send_data());
    EXPECT_FALSE(stream.can_receive_data());
}