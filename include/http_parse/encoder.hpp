#pragma once

#include "core.hpp"
#include "buffer.hpp"
#include "v1/encoder.hpp"
#include "v2/encoder.hpp"
#include <memory>
#include <unordered_map>

namespace co::http {

// =============================================================================
// Unified Encoder Interface
// =============================================================================

class encoder {
public:
    explicit encoder(version ver = version::http_1_1);
    ~encoder();
    
    // Non-copyable, movable
    encoder(const encoder&) = delete;
    encoder& operator=(const encoder&) = delete;
    encoder(encoder&&) noexcept;
    encoder& operator=(encoder&&) noexcept;
    
    // =============================================================================
    // HTTP/1.x Encoding Interface
    // =============================================================================
    
    // Encode to string (convenience)
    std::expected<std::string, error_code> encode_request(const request& req);
    std::expected<std::string, error_code> encode_response(const response& resp);
    
    // High-performance encoding to buffer
    std::expected<size_t, error_code> encode_request(const request& req, output_buffer& output);
    std::expected<size_t, error_code> encode_response(const response& resp, output_buffer& output);
    
    // =============================================================================
    // HTTP/2 Encoding Interface
    // =============================================================================
    
    // Encode HTTP/2 messages
    std::expected<size_t, error_code> encode_h2_request(uint32_t stream_id, const request& req, 
                                                       output_buffer& output, bool end_stream = true);
    std::expected<size_t, error_code> encode_h2_response(uint32_t stream_id, const response& resp, 
                                                        output_buffer& output, bool end_stream = true);
    
    // Encode HTTP/2 data frames
    std::expected<size_t, error_code> encode_h2_data(uint32_t stream_id, std::span<const uint8_t> data, 
                                                     output_buffer& output, bool end_stream = false);
    std::expected<size_t, error_code> encode_h2_data(uint32_t stream_id, std::string_view data, 
                                                     output_buffer& output, bool end_stream = false);
    
    // Encode HTTP/2 control frames
    std::expected<size_t, error_code> encode_h2_settings(const std::unordered_map<uint16_t, uint32_t>& settings, 
                                                         output_buffer& output, bool ack = false);
    std::expected<size_t, error_code> encode_h2_ping(std::span<const uint8_t, 8> data, 
                                                     output_buffer& output, bool ack = false);
    std::expected<size_t, error_code> encode_h2_goaway(uint32_t last_stream_id, error_code error, 
                                                       std::string_view debug_data, output_buffer& output);
    std::expected<size_t, error_code> encode_h2_window_update(uint32_t stream_id, uint32_t increment, 
                                                             output_buffer& output);
    std::expected<size_t, error_code> encode_h2_rst_stream(uint32_t stream_id, error_code error, 
                                                          output_buffer& output);
    std::expected<size_t, error_code> encode_h2_priority(uint32_t stream_id, uint32_t dependent_stream_id, 
                                                        uint8_t weight, bool exclusive, output_buffer& output);
    std::expected<size_t, error_code> encode_h2_push_promise(uint32_t stream_id, uint32_t promised_stream_id,
                                                            const std::vector<header>& headers, output_buffer& output);
    
    // Connection establishment
    std::expected<size_t, error_code> encode_h2_preface(output_buffer& output);
    
    // =============================================================================
    // Configuration
    // =============================================================================
    
    // HPACK compression settings
    void set_hpack_compression_enabled(bool enabled);
    void set_hpack_dynamic_table_size(uint32_t size);
    bool get_hpack_compression_enabled() const noexcept;
    uint32_t get_hpack_dynamic_table_size() const noexcept;
    
    // Performance tuning
    void set_max_frame_size(uint32_t size);
    uint32_t get_max_frame_size() const noexcept;
    
private:
    class impl;
    std::unique_ptr<impl> pimpl_;
    
    version version_;
    std::unique_ptr<v1::encoder> v1_encoder_;
    std::unique_ptr<v2::encoder> v2_encoder_;
};

} // namespace co::http

// Include implementation
#include "detail/encoder_impl.hpp"