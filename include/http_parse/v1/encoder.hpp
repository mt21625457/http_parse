#pragma once

#include "../core.hpp"
#include "../buffer.hpp"

namespace co::http::v1 {

// =============================================================================
// HTTP/1.x Encoder Interface
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
    
    // Encode to string (convenience)
    std::expected<std::string, error_code> encode_request(const request& req);
    std::expected<std::string, error_code> encode_response(const response& resp);
    
    // High-performance encoding to buffer
    std::expected<size_t, error_code> encode_request(const request& req, output_buffer& output);
    std::expected<size_t, error_code> encode_response(const response& resp, output_buffer& output);
    
    // Configuration
    void set_version(version ver) { version_ = ver; }
    version get_version() const noexcept { return version_; }
    
private:
    version version_ = version::http_1_1;
};

} // namespace co::http::v1

// Include implementation
#include "encoder_impl.hpp"