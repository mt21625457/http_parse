#pragma once

#include "../core.hpp"
#include <functional>
#include <unordered_map>

namespace co::http::v2 {

// =============================================================================
// HTTP/2 Parser Interface
// =============================================================================

class parser {
public:
    parser() = default;
    ~parser() = default;
    
    // Non-copyable, movable
    parser(const parser&) = delete;
    parser& operator=(const parser&) = delete;
    parser(parser&&) = default;
    parser& operator=(parser&&) = default;
    
    // Stream-based callbacks for HTTP/2
    using stream_request_callback = std::function<void(uint32_t stream_id, const request& req)>;
    using stream_response_callback = std::function<void(uint32_t stream_id, const response& resp)>;
    using stream_data_callback = std::function<void(uint32_t stream_id, std::span<const uint8_t> data, bool end_stream)>;
    using stream_error_callback = std::function<void(uint32_t stream_id, error_code error)>;
    using connection_error_callback = std::function<void(error_code error, std::string_view debug_info)>;
    using settings_callback = std::function<void(const std::unordered_map<uint16_t, uint32_t>& settings)>;
    using ping_callback = std::function<void(std::span<const uint8_t, 8> data, bool ack)>;
    using goaway_callback = std::function<void(uint32_t last_stream_id, error_code error, std::string_view debug_data)>;
    
    // Set HTTP/2 callbacks
    void set_stream_request_callback(stream_request_callback callback);
    void set_stream_response_callback(stream_response_callback callback);
    void set_stream_data_callback(stream_data_callback callback);
    void set_stream_error_callback(stream_error_callback callback);
    void set_connection_error_callback(connection_error_callback callback);
    void set_settings_callback(settings_callback callback);
    void set_ping_callback(ping_callback callback);
    void set_goaway_callback(goaway_callback callback);
    
    // Parse HTTP/2 frames
    std::expected<size_t, error_code> parse_frames(std::span<const uint8_t> data);
    
    // Parse connection preface
    std::expected<size_t, error_code> parse_preface(std::span<const uint8_t> data);
    
    // HTTP/2 Configuration
    void set_max_frame_size(uint32_t size) { max_frame_size_ = size; }
    void set_max_header_list_size(uint32_t size) { max_header_list_size_ = size; }
    uint32_t get_max_frame_size() const noexcept { return max_frame_size_; }
    uint32_t get_max_header_list_size() const noexcept { return max_header_list_size_; }
    
private:
    class impl;
    std::unique_ptr<impl> pimpl_;
    
    // HTTP/2 callbacks
    stream_request_callback stream_request_cb_;
    stream_response_callback stream_response_cb_;
    stream_data_callback stream_data_cb_;
    stream_error_callback stream_error_cb_;
    connection_error_callback connection_error_cb_;
    settings_callback settings_cb_;
    ping_callback ping_cb_;
    goaway_callback goaway_cb_;
    
    // HTTP/2 settings
    uint32_t max_frame_size_ = 16384;
    uint32_t max_header_list_size_ = 8192;
};

} // namespace co::http::v2

// Include implementation
#include "parser_impl.hpp"