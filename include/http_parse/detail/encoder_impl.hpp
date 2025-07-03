#pragma once

#include "../encoder.hpp"
#include "../core.hpp"
#include "hpack_impl.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace co::http {

// =============================================================================
// Encoder Implementation Details
// =============================================================================

class encoder::impl {
public:
    version version_ = version::http_1_1;
    
    // HPACK compression settings
    bool hpack_compression_enabled_ = true;
    uint32_t hpack_dynamic_table_size_ = 4096;
    
    // HTTP/2 settings
    uint32_t max_frame_size_ = 16384;
    
    // HPACK encoder
    detail::hpack_encoder hpack_encoder_;
    
    impl(version ver) : version_(ver) {
        hpack_encoder_.set_dynamic_table_size(hpack_dynamic_table_size_);
    }
};

// =============================================================================
// Encoder Public Interface Implementation
// =============================================================================

inline encoder::encoder(version ver) : pimpl_(std::make_unique<impl>(ver)) {}

inline encoder::~encoder() = default;

inline encoder::encoder(encoder&&) noexcept = default;
inline encoder& encoder::operator=(encoder&&) noexcept = default;

// =============================================================================
// HTTP/1.x Encoding Implementation
// =============================================================================

namespace detail {

inline std::string encode_http1_request(const request& req) {
    std::ostringstream oss;
    
    // Request line
    oss << req.get_method_string() << " " << req.uri << " ";
    switch (req.protocol_version) {
        case version::http_1_0: oss << "HTTP/1.0"; break;
        case version::http_1_1: oss << "HTTP/1.1"; break;
        default: oss << "HTTP/1.1"; break;
    }
    oss << "\r\n";
    
    // Headers
    for (const auto& header : req.headers) {
        oss << header.name << ": " << header.value << "\r\n";
    }
    
    // Add content-length if body exists and not already set
    if (!req.body.empty() && !req.has_header("content-length")) {
        oss << "Content-Length: " << req.body.size() << "\r\n";
    }
    
    // End headers
    oss << "\r\n";
    
    // Body
    if (!req.body.empty()) {
        oss << req.body;
    }
    
    return oss.str();
}

inline std::string encode_http1_response(const response& resp) {
    std::ostringstream oss;
    
    // Status line
    switch (resp.protocol_version) {
        case version::http_1_0: oss << "HTTP/1.0"; break;
        case version::http_1_1: oss << "HTTP/1.1"; break;
        default: oss << "HTTP/1.1"; break;
    }
    oss << " " << resp.status_code << " " << resp.reason_phrase << "\r\n";
    
    // Headers
    for (const auto& header : resp.headers) {
        oss << header.name << ": " << header.value << "\r\n";
    }
    
    // Add content-length if body exists and not already set
    if (!resp.body.empty() && !resp.has_header("content-length")) {
        oss << "Content-Length: " << resp.body.size() << "\r\n";
    }
    
    // End headers
    oss << "\r\n";
    
    // Body
    if (!resp.body.empty()) {
        oss << resp.body;
    }
    
    return oss.str();
}

inline size_t encode_http1_request_to_buffer(const request& req, output_buffer& output) {
    auto encoded = encode_http1_request(req);
    size_t size = encoded.size();
    output.append(encoded);
    return size;
}

inline size_t encode_http1_response_to_buffer(const response& resp, output_buffer& output) {
    auto encoded = encode_http1_response(resp);
    size_t size = encoded.size();
    output.append(encoded);
    return size;
}

// =============================================================================
// HTTP/2 Frame Encoding Utilities
// =============================================================================

inline void write_uint32_be(uint8_t* buffer, uint32_t value) {
    buffer[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[3] = static_cast<uint8_t>(value & 0xFF);
}

inline void write_uint24_be(uint8_t* buffer, uint32_t value) {
    buffer[0] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[2] = static_cast<uint8_t>(value & 0xFF);
}

inline void write_frame_header(output_buffer& output, uint32_t length, uint8_t type, uint8_t flags, uint32_t stream_id) {
    uint8_t header[9];
    write_uint24_be(header, length);
    header[3] = type;
    header[4] = flags;
    write_uint32_be(header + 5, stream_id & 0x7FFFFFFF); // Clear reserved bit
    output.append(reinterpret_cast<const char*>(header), 9);
}

inline size_t encode_h2_settings_frame(const std::unordered_map<uint16_t, uint32_t>& settings, 
                                     output_buffer& output, bool ack) {
    size_t initial_size = output.size();
    
    uint32_t payload_length = ack ? 0 : static_cast<uint32_t>(settings.size() * 6);
    uint8_t flags = ack ? 0x01 : 0x00; // ACK flag
    
    write_frame_header(output, payload_length, 0x04, flags, 0); // SETTINGS frame
    
    if (!ack) {
        for (const auto& [id, value] : settings) {
            uint8_t setting[6];
            setting[0] = static_cast<uint8_t>((id >> 8) & 0xFF);
            setting[1] = static_cast<uint8_t>(id & 0xFF);
            write_uint32_be(setting + 2, value);
            output.append(reinterpret_cast<const char*>(setting), 6);
        }
    }
    
    return output.size() - initial_size;
}

inline size_t encode_h2_ping_frame(std::span<const uint8_t, 8> data, output_buffer& output, bool ack) {
    size_t initial_size = output.size();
    
    uint8_t flags = ack ? 0x01 : 0x00; // ACK flag
    write_frame_header(output, 8, 0x06, flags, 0); // PING frame
    output.append(data);
    
    return output.size() - initial_size;
}

inline size_t encode_h2_goaway_frame(uint32_t last_stream_id, error_code error, 
                                   std::string_view debug_data, output_buffer& output) {
    size_t initial_size = output.size();
    
    uint32_t payload_length = 8 + static_cast<uint32_t>(debug_data.size());
    write_frame_header(output, payload_length, 0x07, 0x00, 0); // GOAWAY frame
    
    uint8_t payload[8];
    write_uint32_be(payload, last_stream_id & 0x7FFFFFFF);
    write_uint32_be(payload + 4, static_cast<uint32_t>(error));
    output.append(reinterpret_cast<const char*>(payload), 8);
    
    if (!debug_data.empty()) {
        output.append(debug_data);
    }
    
    return output.size() - initial_size;
}

inline size_t encode_h2_window_update_frame(uint32_t stream_id, uint32_t increment, output_buffer& output) {
    size_t initial_size = output.size();
    
    write_frame_header(output, 4, 0x08, 0x00, stream_id); // WINDOW_UPDATE frame
    
    uint8_t payload[4];
    write_uint32_be(payload, increment & 0x7FFFFFFF); // Clear reserved bit
    output.append(reinterpret_cast<const char*>(payload), 4);
    
    return output.size() - initial_size;
}

inline size_t encode_h2_rst_stream_frame(uint32_t stream_id, error_code error, output_buffer& output) {
    size_t initial_size = output.size();
    
    write_frame_header(output, 4, 0x03, 0x00, stream_id); // RST_STREAM frame
    
    uint8_t payload[4];
    write_uint32_be(payload, static_cast<uint32_t>(error));
    output.append(reinterpret_cast<const char*>(payload), 4);
    
    return output.size() - initial_size;
}

inline size_t encode_h2_data_frame(uint32_t stream_id, std::span<const uint8_t> data, 
                                 output_buffer& output, bool end_stream) {
    size_t initial_size = output.size();
    
    uint8_t flags = end_stream ? 0x01 : 0x00; // END_STREAM flag
    write_frame_header(output, static_cast<uint32_t>(data.size()), 0x00, flags, stream_id); // DATA frame
    output.append(data);
    
    return output.size() - initial_size;
}

inline size_t encode_h2_preface(output_buffer& output) {
    size_t initial_size = output.size();
    const std::string_view preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    output.append(preface);
    return output.size() - initial_size;
}

} // namespace detail

// =============================================================================
// HTTP/1.x Encoding Interface Implementation
// =============================================================================

inline std::expected<std::string, error_code> encoder::encode_request(const request& req) {
    try {
        return detail::encode_http1_request(req);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<std::string, error_code> encoder::encode_response(const response& resp) {
    try {
        return detail::encode_http1_response(resp);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_request(const request& req, output_buffer& output) {
    try {
        return detail::encode_http1_request_to_buffer(req, output);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_response(const response& resp, output_buffer& output) {
    try {
        return detail::encode_http1_response_to_buffer(resp, output);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

// =============================================================================
// HTTP/2 Encoding Interface Implementation
// =============================================================================

inline std::expected<size_t, error_code> encoder::encode_h2_request(uint32_t stream_id, const request& req, 
                                                                   output_buffer& output, bool end_stream) {
    try {
        size_t initial_size = output.size();
        
        // Create HTTP/2 pseudo-headers and regular headers
        std::vector<header> h2_headers;
        h2_headers.emplace_back(":method", req.get_method_string());
        h2_headers.emplace_back(":path", req.uri.empty() ? "/" : req.uri);
        h2_headers.emplace_back(":scheme", "https"); // Default to https
        
        // Add regular headers
        for (const auto& hdr : req.headers) {
            h2_headers.push_back(hdr);
        }
        
        // Encode headers using HPACK if enabled
        output_buffer headers_buffer;
        if (pimpl_->hpack_compression_enabled_) {
            auto result = pimpl_->hpack_encoder_.encode_headers(h2_headers, headers_buffer);
            if (!result) return std::unexpected(result.error());
        } else {
            // Fallback to simple encoding without compression
            for (const auto& hdr : h2_headers) {
                headers_buffer.append(hdr.name);
                headers_buffer.append(": ");
                headers_buffer.append(hdr.value);
                headers_buffer.append("\r\n");
            }
        }
        
        // Create HEADERS frame
        uint8_t flags = end_stream ? 0x05 : 0x04; // END_HEADERS | END_STREAM (if needed)
        detail::write_frame_header(output, static_cast<uint32_t>(headers_buffer.size()), 0x01, flags, stream_id);
        output.append(headers_buffer.string_view());
        
        return output.size() - initial_size;
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_response(uint32_t stream_id, const response& resp, 
                                                                    output_buffer& output, bool end_stream) {
    try {
        size_t initial_size = output.size();
        
        // Create HTTP/2 pseudo-headers and regular headers
        std::vector<header> h2_headers;
        h2_headers.emplace_back(":status", std::to_string(resp.status_code));
        
        // Add regular headers
        for (const auto& hdr : resp.headers) {
            h2_headers.push_back(hdr);
        }
        
        // Encode headers using HPACK if enabled
        output_buffer headers_buffer;
        if (pimpl_->hpack_compression_enabled_) {
            auto result = pimpl_->hpack_encoder_.encode_headers(h2_headers, headers_buffer);
            if (!result) return std::unexpected(result.error());
        } else {
            // Fallback to simple encoding without compression
            for (const auto& hdr : h2_headers) {
                headers_buffer.append(hdr.name);
                headers_buffer.append(": ");
                headers_buffer.append(hdr.value);
                headers_buffer.append("\r\n");
            }
        }
        
        // Create HEADERS frame
        uint8_t flags = end_stream ? 0x05 : 0x04; // END_HEADERS | END_STREAM (if needed)
        detail::write_frame_header(output, static_cast<uint32_t>(headers_buffer.size()), 0x01, flags, stream_id);
        output.append(headers_buffer.string_view());
        
        return output.size() - initial_size;
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_data(uint32_t stream_id, std::span<const uint8_t> data, 
                                                                output_buffer& output, bool end_stream) {
    try {
        return detail::encode_h2_data_frame(stream_id, data, output, end_stream);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_data(uint32_t stream_id, std::string_view data, 
                                                                output_buffer& output, bool end_stream) {
    std::span<const uint8_t> data_span{
        reinterpret_cast<const uint8_t*>(data.data()), 
        data.size()
    };
    return encode_h2_data(stream_id, data_span, output, end_stream);
}

inline std::expected<size_t, error_code> encoder::encode_h2_settings(const std::unordered_map<uint16_t, uint32_t>& settings, 
                                                                     output_buffer& output, bool ack) {
    try {
        return detail::encode_h2_settings_frame(settings, output, ack);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_ping(std::span<const uint8_t, 8> data, 
                                                                output_buffer& output, bool ack) {
    try {
        return detail::encode_h2_ping_frame(data, output, ack);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_goaway(uint32_t last_stream_id, error_code error, 
                                                                  std::string_view debug_data, output_buffer& output) {
    try {
        return detail::encode_h2_goaway_frame(last_stream_id, error, debug_data, output);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_window_update(uint32_t stream_id, uint32_t increment, 
                                                                         output_buffer& output) {
    try {
        return detail::encode_h2_window_update_frame(stream_id, increment, output);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_rst_stream(uint32_t stream_id, error_code error, 
                                                                      output_buffer& output) {
    try {
        return detail::encode_h2_rst_stream_frame(stream_id, error, output);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_h2_priority(uint32_t stream_id, uint32_t dependent_stream_id, 
                                                                    uint8_t weight, bool exclusive, output_buffer& output) {
    // PRIORITY frame encoding - simplified
    return std::unexpected(error_code::protocol_error); // TODO: Implement PRIORITY frame
}

inline std::expected<size_t, error_code> encoder::encode_h2_push_promise(uint32_t stream_id, uint32_t promised_stream_id,
                                                                        const std::vector<header>& headers, output_buffer& output) {
    // PUSH_PROMISE frame encoding - simplified
    return std::unexpected(error_code::protocol_error); // TODO: Implement PUSH_PROMISE frame
}

inline std::expected<size_t, error_code> encoder::encode_h2_preface(output_buffer& output) {
    try {
        return detail::encode_h2_preface(output);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

// =============================================================================
// Configuration Implementation
// =============================================================================

inline void encoder::set_hpack_compression_enabled(bool enabled) {
    pimpl_->hpack_compression_enabled_ = enabled;
}

inline void encoder::set_hpack_dynamic_table_size(uint32_t size) {
    pimpl_->hpack_dynamic_table_size_ = size;
    pimpl_->hpack_encoder_.set_dynamic_table_size(size);
}

inline bool encoder::get_hpack_compression_enabled() const noexcept {
    return pimpl_->hpack_compression_enabled_;
}

inline uint32_t encoder::get_hpack_dynamic_table_size() const noexcept {
    return pimpl_->hpack_dynamic_table_size_;
}

inline void encoder::set_max_frame_size(uint32_t size) {
    pimpl_->max_frame_size_ = size;
}

inline uint32_t encoder::get_max_frame_size() const noexcept {
    return pimpl_->max_frame_size_;
}

} // namespace co::http