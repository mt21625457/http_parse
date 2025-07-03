#pragma once

#include "../core.hpp"
#include "../buffer.hpp"
#include <memory>
#include <unordered_map>

namespace co::http::v2 {

// =============================================================================
// HTTP/2 Encoder Interface
// =============================================================================

class encoder {
public:
    encoder() = default;
    ~encoder() = default;
    
    // Non-copyable, movable
    encoder(const encoder&) = delete;
    encoder& operator=(const encoder&) = delete;
    encoder(encoder&&) = default;
    encoder& operator=(encoder&&) = default;
    
    // Encode HTTP/2 messages
    std::expected<size_t, error_code> encode_request(uint32_t stream_id, const request& req, 
                                                   output_buffer& output, bool end_stream = true);
    std::expected<size_t, error_code> encode_response(uint32_t stream_id, const response& resp, 
                                                    output_buffer& output, bool end_stream = true);
    
    // Encode HTTP/2 data frames
    std::expected<size_t, error_code> encode_data(uint32_t stream_id, std::span<const uint8_t> data, 
                                                output_buffer& output, bool end_stream = false);
    std::expected<size_t, error_code> encode_data(uint32_t stream_id, std::string_view data, 
                                                output_buffer& output, bool end_stream = false);
    
    // Encode HTTP/2 control frames
    std::expected<size_t, error_code> encode_settings(const std::unordered_map<uint16_t, uint32_t>& settings, 
                                                     output_buffer& output, bool ack = false);
    std::expected<size_t, error_code> encode_ping(std::span<const uint8_t, 8> data, 
                                                 output_buffer& output, bool ack = false);
    std::expected<size_t, error_code> encode_goaway(uint32_t last_stream_id, error_code error, 
                                                   std::string_view debug_data, output_buffer& output);
    std::expected<size_t, error_code> encode_window_update(uint32_t stream_id, uint32_t increment, 
                                                         output_buffer& output);
    std::expected<size_t, error_code> encode_rst_stream(uint32_t stream_id, error_code error, 
                                                      output_buffer& output);
    std::expected<size_t, error_code> encode_priority(uint32_t stream_id, uint32_t dependent_stream_id, 
                                                    uint8_t weight, bool exclusive, output_buffer& output);
    std::expected<size_t, error_code> encode_push_promise(uint32_t stream_id, uint32_t promised_stream_id,
                                                        const std::vector<header>& headers, output_buffer& output);
    
    // Connection establishment
    std::expected<size_t, error_code> encode_preface(output_buffer& output);
    
    // HPACK configuration
    void set_hpack_compression_enabled(bool enabled) { hpack_compression_enabled_ = enabled; }
    void set_hpack_dynamic_table_size(uint32_t size);
    bool get_hpack_compression_enabled() const noexcept { return hpack_compression_enabled_; }
    uint32_t get_hpack_dynamic_table_size() const noexcept { return hpack_dynamic_table_size_; }
    
    // Performance tuning
    void set_max_frame_size(uint32_t size) { max_frame_size_ = size; }
    uint32_t get_max_frame_size() const noexcept { return max_frame_size_; }
    
private:
    class impl;
    std::unique_ptr<impl> pimpl_;
    
    // Configuration
    bool hpack_compression_enabled_ = true;
    uint32_t hpack_dynamic_table_size_ = 4096;
    uint32_t max_frame_size_ = 16384;
};

} // namespace co::http::v2

// Include implementation
#include "encoder_impl.hpp"