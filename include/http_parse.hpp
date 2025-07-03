#pragma once

// =============================================================================
// HTTP Parse Library - Unified API
// A pure C++23 header-only HTTP/1.x and HTTP/2 protocol parsing library
// =============================================================================

#include "http_parse/core.hpp"
#include "http_parse/buffer.hpp"
#include "http_parse/parser.hpp"
#include "http_parse/encoder.hpp"
#include "http_parse/builder.hpp"

namespace co::http {

// =============================================================================
// Unified HTTP Parse Interface
// =============================================================================

class http_parse {
public:
    http_parse() = default;
    
    // =============================================================================
    // HTTP/1.x Parsing Interface
    // =============================================================================
    
    // Parse complete messages
    static std::expected<request, error_code> parse_request(std::string_view data, version ver = version::auto_detect) {
        parser p(ver);
        return p.parse_request(data);
    }
    
    static std::expected<response, error_code> parse_response(std::string_view data, version ver = version::auto_detect) {
        parser p(ver);
        return p.parse_response(data);
    }
    
    // Incremental parsing
    template<typename MessageType>
    class incremental_parser {
    public:
        explicit incremental_parser(version ver = version::auto_detect) : parser_(ver) {}
        
        std::expected<size_t, error_code> parse(std::span<const char> data, MessageType& msg) {
            if constexpr (std::is_same_v<MessageType, request>) {
                return parser_.parse_request_incremental(data, msg);
            } else {
                return parser_.parse_response_incremental(data, msg);
            }
        }
        
        bool is_complete() const noexcept { return parser_.is_parse_complete(); }
        bool needs_more_data() const noexcept { return parser_.needs_more_data(); }
        version detected_version() const noexcept { return parser_.detected_version(); }
        void reset() noexcept { parser_.reset(); }
        
    private:
        parser parser_;
    };
    
    using request_parser = incremental_parser<request>;
    using response_parser = incremental_parser<response>;
    
    // =============================================================================
    // HTTP/1.x Encoding Interface
    // =============================================================================
    
    // Encode to string (convenience)
    static std::expected<std::string, error_code> encode_request(const request& req, version ver = version::http_1_1) {
        encoder enc(ver);
        return enc.encode_request(req);
    }
    
    static std::expected<std::string, error_code> encode_response(const response& resp, version ver = version::http_1_1) {
        encoder enc(ver);
        return enc.encode_response(resp);
    }
    
    // High-performance encoding to buffer
    static std::expected<size_t, error_code> encode_request(const request& req, output_buffer& output, version ver = version::http_1_1) {
        encoder enc(ver);
        return enc.encode_request(req, output);
    }
    
    static std::expected<size_t, error_code> encode_response(const response& resp, output_buffer& output, version ver = version::http_1_1) {
        encoder enc(ver);
        return enc.encode_response(resp, output);
    }
    
    // =============================================================================
    // HTTP/2 Parsing Interface
    // =============================================================================
    
    class h2_parser {
    public:
        h2_parser() : parser_(version::http_2_0) {}
        
        // Stream-based callbacks
        using stream_request_callback = parser::stream_request_callback;
        using stream_response_callback = parser::stream_response_callback;
        using stream_data_callback = parser::stream_data_callback;
        using stream_error_callback = parser::stream_error_callback;
        using connection_error_callback = parser::connection_error_callback;
        using settings_callback = parser::settings_callback;
        using ping_callback = parser::ping_callback;
        using goaway_callback = parser::goaway_callback;
        
        // Set callbacks
        void set_stream_request_callback(stream_request_callback cb) { parser_.set_stream_request_callback(std::move(cb)); }
        void set_stream_response_callback(stream_response_callback cb) { parser_.set_stream_response_callback(std::move(cb)); }
        void set_stream_data_callback(stream_data_callback cb) { parser_.set_stream_data_callback(std::move(cb)); }
        void set_stream_error_callback(stream_error_callback cb) { parser_.set_stream_error_callback(std::move(cb)); }
        void set_connection_error_callback(connection_error_callback cb) { parser_.set_connection_error_callback(std::move(cb)); }
        void set_settings_callback(settings_callback cb) { parser_.set_settings_callback(std::move(cb)); }
        void set_ping_callback(ping_callback cb) { parser_.set_ping_callback(std::move(cb)); }
        void set_goaway_callback(goaway_callback cb) { parser_.set_goaway_callback(std::move(cb)); }
        
        // Parse frames
        std::expected<size_t, error_code> parse_frames(std::span<const uint8_t> data) {
            return parser_.parse_h2_frames(data);
        }
        
        std::expected<size_t, error_code> parse_preface(std::span<const uint8_t> data) {
            return parser_.parse_h2_preface(data);
        }
        
        // Configuration
        void set_max_frame_size(uint32_t size) { parser_.set_h2_max_frame_size(size); }
        void set_max_header_list_size(uint32_t size) { parser_.set_h2_max_header_list_size(size); }
        uint32_t get_max_frame_size() const noexcept { return parser_.get_h2_max_frame_size(); }
        uint32_t get_max_header_list_size() const noexcept { return parser_.get_h2_max_header_list_size(); }
        
    private:
        parser parser_;
    };
    
    // =============================================================================
    // HTTP/2 Encoding Interface
    // =============================================================================
    
    class h2_encoder {
    public:
        h2_encoder() : encoder_(version::http_2_0) {}
        
        // Encode HTTP/2 messages
        std::expected<size_t, error_code> encode_request(uint32_t stream_id, const request& req, output_buffer& output, bool end_stream = true) {
            return encoder_.encode_h2_request(stream_id, req, output, end_stream);
        }
        
        std::expected<size_t, error_code> encode_response(uint32_t stream_id, const response& resp, output_buffer& output, bool end_stream = true) {
            return encoder_.encode_h2_response(stream_id, resp, output, end_stream);
        }
        
        // Encode HTTP/2 data frames
        std::expected<size_t, error_code> encode_data(uint32_t stream_id, std::span<const uint8_t> data, output_buffer& output, bool end_stream = false) {
            return encoder_.encode_h2_data(stream_id, data, output, end_stream);
        }
        
        std::expected<size_t, error_code> encode_data(uint32_t stream_id, std::string_view data, output_buffer& output, bool end_stream = false) {
            return encoder_.encode_h2_data(stream_id, data, output, end_stream);
        }
        
        // Encode HTTP/2 control frames
        std::expected<size_t, error_code> encode_settings(const std::unordered_map<uint16_t, uint32_t>& settings, output_buffer& output, bool ack = false) {
            return encoder_.encode_h2_settings(settings, output, ack);
        }
        
        std::expected<size_t, error_code> encode_ping(std::span<const uint8_t, 8> data, output_buffer& output, bool ack = false) {
            return encoder_.encode_h2_ping(data, output, ack);
        }
        
        std::expected<size_t, error_code> encode_goaway(uint32_t last_stream_id, error_code error, std::string_view debug_data, output_buffer& output) {
            return encoder_.encode_h2_goaway(last_stream_id, error, debug_data, output);
        }
        
        std::expected<size_t, error_code> encode_window_update(uint32_t stream_id, uint32_t increment, output_buffer& output) {
            return encoder_.encode_h2_window_update(stream_id, increment, output);
        }
        
        std::expected<size_t, error_code> encode_rst_stream(uint32_t stream_id, error_code error, output_buffer& output) {
            return encoder_.encode_h2_rst_stream(stream_id, error, output);
        }
        
        std::expected<size_t, error_code> encode_preface(output_buffer& output) {
            return encoder_.encode_h2_preface(output);
        }
        
        // HPACK configuration
        void set_hpack_compression_enabled(bool enabled) { encoder_.set_hpack_compression_enabled(enabled); }
        void set_hpack_dynamic_table_size(uint32_t size) { encoder_.set_hpack_dynamic_table_size(size); }
        bool get_hpack_compression_enabled() const noexcept { return encoder_.get_hpack_compression_enabled(); }
        uint32_t get_hpack_dynamic_table_size() const noexcept { return encoder_.get_hpack_dynamic_table_size(); }
        
        // Performance tuning
        void set_max_frame_size(uint32_t size) { encoder_.set_max_frame_size(size); }
        uint32_t get_max_frame_size() const noexcept { return encoder_.get_max_frame_size(); }
        
    private:
        encoder encoder_;
    };
    
    // =============================================================================
    // Builder Pattern Interface
    // =============================================================================
    
    static request_builder build_request() {
        return request_builder{};
    }
    
    static response_builder build_response() {
        return response_builder{};
    }
    
    // =============================================================================
    // Utility Functions
    // =============================================================================
    
    static std::string version_string(version v) {
        switch (v) {
            case version::http_1_0: return "HTTP/1.0";
            case version::http_1_1: return "HTTP/1.1"; 
            case version::http_2_0: return "HTTP/2.0";
            case version::auto_detect: return "AUTO";
        }
        return "UNKNOWN";
    }
    
    static std::string method_string(method m) {
        switch (m) {
            case method::get: return "GET";
            case method::post: return "POST";
            case method::put: return "PUT";
            case method::delete_: return "DELETE";
            case method::head: return "HEAD";
            case method::options: return "OPTIONS";
            case method::trace: return "TRACE";
            case method::connect: return "CONNECT";
            case method::patch: return "PATCH";
            case method::unknown: return "UNKNOWN";
        }
        return "UNKNOWN";
    }
    
    static std::string error_string(error_code e) {
        switch (e) {
            case error_code::success: return "Success";
            case error_code::need_more_data: return "Need more data";
            case error_code::protocol_error: return "Protocol error";
            case error_code::invalid_method: return "Invalid method";
            case error_code::invalid_uri: return "Invalid URI";
            case error_code::invalid_version: return "Invalid version";
            case error_code::invalid_header: return "Invalid header";
            case error_code::invalid_body: return "Invalid body";
            case error_code::frame_size_error: return "Frame size error";
            case error_code::compression_error: return "Compression error";
            case error_code::flow_control_error: return "Flow control error";
            case error_code::stream_closed: return "Stream closed";
            case error_code::connection_error: return "Connection error";
        }
        return "Unknown error";
    }
};

} // namespace co::http