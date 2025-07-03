#pragma once

#include "../core.hpp"
#include <cstdint>
#include <array>

namespace co::http::v2 {

// =============================================================================
// HTTP/2 Protocol Constants (RFC 7540)
// =============================================================================

// Frame types
enum class frame_type : uint8_t {
    data = 0x00,
    headers = 0x01,
    priority = 0x02,
    rst_stream = 0x03,
    settings = 0x04,
    push_promise = 0x05,
    ping = 0x06,
    goaway = 0x07,
    window_update = 0x08,
    continuation = 0x09
};

// Frame flags
enum class frame_flags : uint8_t {
    none = 0x00,
    end_stream = 0x01,
    ack = 0x01,
    end_headers = 0x04,
    padded = 0x08,
    priority_flag = 0x20
};

// Error codes (RFC 7540 Section 7)
enum class h2_error_code : uint32_t {
    no_error = 0x00,
    protocol_error = 0x01,
    internal_error = 0x02,
    flow_control_error = 0x03,
    settings_timeout = 0x04,
    stream_closed = 0x05,
    frame_size_error = 0x06,
    refused_stream = 0x07,
    cancel = 0x08,
    compression_error = 0x09,
    connect_error = 0x0a,
    enhance_your_calm = 0x0b,
    inadequate_security = 0x0c,
    http_1_1_required = 0x0d
};

// Settings identifiers (RFC 7540 Section 6.5.2)
enum class settings_id : uint16_t {
    header_table_size = 0x01,
    enable_push = 0x02,
    max_concurrent_streams = 0x03,
    initial_window_size = 0x04,
    max_frame_size = 0x05,
    max_header_list_size = 0x06
};

// Stream states (RFC 7540 Section 5.1)
enum class stream_state {
    idle,
    reserved_local,
    reserved_remote,
    open,
    half_closed_local,
    half_closed_remote,
    closed
};

// =============================================================================
// HTTP/2 Frame Header
// =============================================================================

struct frame_header {
    uint32_t length : 24;  // 24-bit length
    uint8_t type;          // Frame type
    uint8_t flags;         // Frame flags
    uint32_t stream_id;    // 31-bit stream identifier (R bit reserved)
    
    static constexpr size_t size = 9;
    
    // Parse frame header from raw bytes
    static frame_header parse(const uint8_t* data) noexcept {
        frame_header header;
        header.length = (static_cast<uint32_t>(data[0]) << 16) |
                       (static_cast<uint32_t>(data[1]) << 8) |
                       static_cast<uint32_t>(data[2]);
        header.type = data[3];
        header.flags = data[4];
        header.stream_id = (static_cast<uint32_t>(data[5]) << 24) |
                          (static_cast<uint32_t>(data[6]) << 16) |
                          (static_cast<uint32_t>(data[7]) << 8) |
                          static_cast<uint32_t>(data[8]);
        header.stream_id &= 0x7FFFFFFF; // Clear reserved bit
        return header;
    }
    
    // Serialize frame header to raw bytes
    void serialize(uint8_t* data) const noexcept {
        data[0] = static_cast<uint8_t>((length >> 16) & 0xFF);
        data[1] = static_cast<uint8_t>((length >> 8) & 0xFF);
        data[2] = static_cast<uint8_t>(length & 0xFF);
        data[3] = type;
        data[4] = flags;
        data[5] = static_cast<uint8_t>((stream_id >> 24) & 0x7F); // Clear reserved bit
        data[6] = static_cast<uint8_t>((stream_id >> 16) & 0xFF);
        data[7] = static_cast<uint8_t>((stream_id >> 8) & 0xFF);
        data[8] = static_cast<uint8_t>(stream_id & 0xFF);
    }
};

// =============================================================================
// HTTP/2 Settings Frame
// =============================================================================

struct settings_frame {
    std::unordered_map<uint16_t, uint32_t> settings;
    bool ack = false;
    
    // Default settings values (RFC 7540 Section 6.5.2)
    static constexpr uint32_t default_header_table_size = 4096;
    static constexpr uint32_t default_enable_push = 1;
    static constexpr uint32_t default_max_concurrent_streams = UINT32_MAX;
    static constexpr uint32_t default_initial_window_size = 65535;
    static constexpr uint32_t default_max_frame_size = 16384;
    static constexpr uint32_t default_max_header_list_size = UINT32_MAX;
};

// =============================================================================
// HTTP/2 Priority Frame
// =============================================================================

struct priority_frame {
    uint32_t stream_dependency;
    uint8_t weight;
    bool exclusive;
};

// =============================================================================
// HTTP/2 Window Update Frame
// =============================================================================

struct window_update_frame {
    uint32_t window_size_increment;
};

// =============================================================================
// HTTP/2 Connection Preface
// =============================================================================

constexpr std::string_view connection_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

// =============================================================================
// HTTP/2 Protocol Limits
// =============================================================================

struct protocol_limits {
    static constexpr uint32_t max_frame_size_limit = (1u << 24) - 1; // 2^24 - 1
    static constexpr uint32_t min_max_frame_size = 16384;
    static constexpr uint32_t max_window_size = (1u << 31) - 1; // 2^31 - 1
    static constexpr uint32_t max_stream_id = (1u << 31) - 1; // 2^31 - 1
    static constexpr uint32_t max_header_list_size_limit = UINT32_MAX;
};

// =============================================================================
// HTTP/2 Stream Information
// =============================================================================

struct stream_info {
    uint32_t id;
    stream_state state = stream_state::idle;
    int32_t window_size = settings_frame::default_initial_window_size;
    int32_t remote_window_size = settings_frame::default_initial_window_size;
    
    // Priority information
    uint32_t dependency = 0;
    uint8_t weight = 16; // Default weight (RFC 7540 Section 5.3.2)
    bool exclusive = false;
    
    // Stream tracking
    bool headers_complete = false;
    bool data_complete = false;
    bool local_closed = false;
    bool remote_closed = false;
    
    // Error tracking
    h2_error_code error = h2_error_code::no_error;
    
    bool is_closed() const noexcept {
        return state == stream_state::closed;
    }
    
    bool is_half_closed_local() const noexcept {
        return state == stream_state::half_closed_local || local_closed;
    }
    
    bool is_half_closed_remote() const noexcept {
        return state == stream_state::half_closed_remote || remote_closed;
    }
    
    bool can_send_data() const noexcept {
        return !is_closed() && !is_half_closed_local() && window_size > 0;
    }
    
    bool can_receive_data() const noexcept {
        return !is_closed() && !is_half_closed_remote();
    }
};

// =============================================================================
// HTTP/2 Connection Settings
// =============================================================================

struct connection_settings {
    uint32_t header_table_size = settings_frame::default_header_table_size;
    bool enable_push = static_cast<bool>(settings_frame::default_enable_push);
    uint32_t max_concurrent_streams = settings_frame::default_max_concurrent_streams;
    uint32_t initial_window_size = settings_frame::default_initial_window_size;
    uint32_t max_frame_size = settings_frame::default_max_frame_size;
    uint32_t max_header_list_size = settings_frame::default_max_header_list_size;
    
    // Connection-level flow control
    int32_t connection_window_size = settings_frame::default_initial_window_size;
    int32_t remote_connection_window_size = settings_frame::default_initial_window_size;
    
    void apply_setting(uint16_t id, uint32_t value) {
        switch (static_cast<settings_id>(id)) {
            case settings_id::header_table_size:
                header_table_size = value;
                break;
            case settings_id::enable_push:
                enable_push = (value != 0);
                break;
            case settings_id::max_concurrent_streams:
                max_concurrent_streams = value;
                break;
            case settings_id::initial_window_size:
                initial_window_size = value;
                break;
            case settings_id::max_frame_size:
                max_frame_size = value;
                break;
            case settings_id::max_header_list_size:
                max_header_list_size = value;
                break;
        }
    }
    
    bool validate_setting(uint16_t id, uint32_t value) const noexcept {
        switch (static_cast<settings_id>(id)) {
            case settings_id::header_table_size:
                return true; // Any value is valid
            case settings_id::enable_push:
                return value <= 1; // Must be 0 or 1
            case settings_id::max_concurrent_streams:
                return true; // Any value is valid
            case settings_id::initial_window_size:
                return value <= protocol_limits::max_window_size;
            case settings_id::max_frame_size:
                return value >= protocol_limits::min_max_frame_size && 
                       value <= protocol_limits::max_frame_size_limit;
            case settings_id::max_header_list_size:
                return true; // Any value is valid
            default:
                return true; // Unknown settings are ignored
        }
    }
};

} // namespace co::http::v2