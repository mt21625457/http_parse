#pragma once

#include "core.hpp"
#include "v1/parser.hpp"
#include "v2/parser.hpp"
#include <memory>
#include <functional>

namespace co::http {

// =============================================================================
// Unified Parser Interface
// =============================================================================

class parser {
public:
    explicit parser(version ver = version::auto_detect);
    ~parser();
    
    // Non-copyable, movable
    parser(const parser&) = delete;
    parser& operator=(const parser&) = delete;
    parser(parser&&) noexcept;
    parser& operator=(parser&&) noexcept;
    
    // =============================================================================
    // HTTP/1.x Parsing Interface
    // =============================================================================
    
    // Parse complete messages
    std::expected<request, error_code> parse_request(std::string_view data);
    std::expected<response, error_code> parse_response(std::string_view data);
    
    // Incremental parsing
    std::expected<size_t, error_code> parse_request_incremental(std::span<const char> data, request& req);
    std::expected<size_t, error_code> parse_response_incremental(std::span<const char> data, response& resp);
    
    // Parsing state
    bool is_parse_complete() const noexcept;
    bool needs_more_data() const noexcept;
    version detected_version() const noexcept;
    void reset() noexcept;
    
    // =============================================================================
    // HTTP/2 Parsing Interface
    // =============================================================================
    
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
    std::expected<size_t, error_code> parse_h2_frames(std::span<const uint8_t> data);
    
    // Parse connection preface
    std::expected<size_t, error_code> parse_h2_preface(std::span<const uint8_t> data);
    
    // HTTP/2 Configuration
    void set_h2_max_frame_size(uint32_t size);
    void set_h2_max_header_list_size(uint32_t size);
    uint32_t get_h2_max_frame_size() const noexcept;
    uint32_t get_h2_max_header_list_size() const noexcept;
    
private:
    class impl;
    std::unique_ptr<impl> pimpl_;
    
    version version_;
    std::unique_ptr<v1::parser> v1_parser_;
    std::unique_ptr<v2::parser> v2_parser_;
};

} // namespace co::http

// Include implementation
#include "detail/parser_impl.hpp"