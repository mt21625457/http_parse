#pragma once

#include "parser.hpp"
#include "../detail/hpack_impl.hpp"
#include <charconv>
#include <memory>

namespace co::http::v2 {

// =============================================================================
// HTTP/2 Parser Implementation Details
// =============================================================================

class parser::impl {
public:
    // HPACK decoder
    detail::hpack_decoder hpack_decoder_;
    
    impl() = default;
};

// =============================================================================
// HTTP/2 Header Conversion Functions
// =============================================================================

namespace detail {

inline request convert_h2_headers_to_request(const std::vector<header>& headers) {
    request req;
    
    for (const auto& hdr : headers) {
        if (hdr.name == ":method") {
            req.set_method(hdr.value);
        } else if (hdr.name == ":path") {
            req.uri = hdr.value;
            req.target = hdr.value;
        } else if (hdr.name == ":scheme") {
            // Store scheme information if needed
        } else if (hdr.name == ":authority") {
            req.add_header("host", hdr.value);
        } else if (!hdr.name.starts_with(':')) {
            // Regular header
            req.headers.push_back(hdr);
        }
    }
    
    req.protocol_version = version::http_2_0;
    return req;
}

inline response convert_h2_headers_to_response(const std::vector<header>& headers) {
    response resp;
    
    for (const auto& hdr : headers) {
        if (hdr.name == ":status") {
            auto result = std::from_chars(hdr.value.data(), hdr.value.data() + hdr.value.size(), resp.status_code);
            if (result.ec != std::errc{}) {
                resp.status_code = 500; // Default to server error
            }
        } else if (!hdr.name.starts_with(':')) {
            // Regular header
            resp.headers.push_back(hdr);
        }
    }
    
    resp.protocol_version = version::http_2_0;
    return resp;
}

} // namespace detail

// =============================================================================
// Public Interface Implementation
// =============================================================================

inline void parser::set_stream_request_callback(stream_request_callback callback) {
    stream_request_cb_ = std::move(callback);
}

inline void parser::set_stream_response_callback(stream_response_callback callback) {
    stream_response_cb_ = std::move(callback);
}

inline void parser::set_stream_data_callback(stream_data_callback callback) {
    stream_data_cb_ = std::move(callback);
}

inline void parser::set_stream_error_callback(stream_error_callback callback) {
    stream_error_cb_ = std::move(callback);
}

inline void parser::set_connection_error_callback(connection_error_callback callback) {
    connection_error_cb_ = std::move(callback);
}

inline void parser::set_settings_callback(settings_callback callback) {
    settings_cb_ = std::move(callback);
}

inline void parser::set_ping_callback(ping_callback callback) {
    ping_cb_ = std::move(callback);
}

inline void parser::set_goaway_callback(goaway_callback callback) {
    goaway_cb_ = std::move(callback);
}

inline std::expected<size_t, error_code> parser::parse_frames(std::span<const uint8_t> data) {
    size_t pos = 0;
    
    while (pos + 9 <= data.size()) { // Frame header is 9 bytes
        // Parse frame header
        uint32_t length = (static_cast<uint32_t>(data[pos]) << 16) |
                         (static_cast<uint32_t>(data[pos + 1]) << 8) |
                         static_cast<uint32_t>(data[pos + 2]);
        uint8_t type = data[pos + 3];
        uint8_t flags = data[pos + 4];
        uint32_t stream_id = (static_cast<uint32_t>(data[pos + 5]) << 24) |
                            (static_cast<uint32_t>(data[pos + 6]) << 16) |
                            (static_cast<uint32_t>(data[pos + 7]) << 8) |
                            static_cast<uint32_t>(data[pos + 8]);
        stream_id &= 0x7FFFFFFF; // Clear reserved bit
        
        pos += 9;
        
        // Check if we have the complete frame payload
        if (pos + length > data.size()) {
            return std::unexpected(error_code::need_more_data);
        }
        
        // Process frame based on type
        std::span<const uint8_t> payload = data.subspan(pos, length);
        
        switch (type) {
            case 0x00: // DATA frame
                if (stream_data_cb_) {
                    bool end_stream = (flags & 0x01) != 0;
                    stream_data_cb_(stream_id, payload, end_stream);
                }
                break;
                
            case 0x01: // HEADERS frame
                try {
                    if (!pimpl_) {
                        pimpl_ = std::make_unique<impl>();
                    }
                    auto headers = pimpl_->hpack_decoder_.decode_headers(payload);
                    if (headers) {
                        // Convert to request/response based on pseudo-headers
                        bool is_request = false;
                        for (const auto& hdr : *headers) {
                            if (hdr.name == ":method") {
                                is_request = true;
                                break;
                            }
                        }
                        
                        if (is_request && stream_request_cb_) {
                            request req = detail::convert_h2_headers_to_request(*headers);
                            stream_request_cb_(stream_id, req);
                        } else if (!is_request && stream_response_cb_) {
                            response resp = detail::convert_h2_headers_to_response(*headers);
                            stream_response_cb_(stream_id, resp);
                        }
                    } else if (stream_error_cb_) {
                        stream_error_cb_(stream_id, headers.error());
                    }
                } catch (...) {
                    if (stream_error_cb_) {
                        stream_error_cb_(stream_id, error_code::compression_error);
                    }
                }
                break;
                
            case 0x04: // SETTINGS frame
                if (settings_cb_) {
                    std::unordered_map<uint16_t, uint32_t> settings;
                    for (size_t i = 0; i + 6 <= payload.size(); i += 6) {
                        uint16_t id = (static_cast<uint16_t>(payload[i]) << 8) | payload[i + 1];
                        uint32_t value = (static_cast<uint32_t>(payload[i + 2]) << 24) |
                                        (static_cast<uint32_t>(payload[i + 3]) << 16) |
                                        (static_cast<uint32_t>(payload[i + 4]) << 8) |
                                        static_cast<uint32_t>(payload[i + 5]);
                        settings[id] = value;
                    }
                    settings_cb_(settings);
                }
                break;
                
            case 0x06: // PING frame
                if (ping_cb_ && payload.size() == 8) {
                    bool ack = (flags & 0x01) != 0;
                    std::span<const uint8_t, 8> ping_data{payload.data(), 8};
                    ping_cb_(ping_data, ack);
                }
                break;
                
            case 0x07: // GOAWAY frame
                if (goaway_cb_ && payload.size() >= 8) {
                    uint32_t last_stream_id = (static_cast<uint32_t>(payload[0]) << 24) |
                                             (static_cast<uint32_t>(payload[1]) << 16) |
                                             (static_cast<uint32_t>(payload[2]) << 8) |
                                             static_cast<uint32_t>(payload[3]);
                    last_stream_id &= 0x7FFFFFFF;
                    
                    uint32_t error_code_val = (static_cast<uint32_t>(payload[4]) << 24) |
                                             (static_cast<uint32_t>(payload[5]) << 16) |
                                             (static_cast<uint32_t>(payload[6]) << 8) |
                                             static_cast<uint32_t>(payload[7]);
                    
                    std::string_view debug_data;
                    if (payload.size() > 8) {
                        debug_data = std::string_view{
                            reinterpret_cast<const char*>(payload.data() + 8),
                            payload.size() - 8
                        };
                    }
                    
                    goaway_cb_(last_stream_id, static_cast<error_code>(error_code_val), debug_data);
                }
                break;
                
            default:
                // Unknown frame type - ignore for now
                break;
        }
        
        pos += length;
    }
    
    return pos;
}

inline std::expected<size_t, error_code> parser::parse_preface(std::span<const uint8_t> data) {
    // HTTP/2 connection preface: "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
    const std::string_view preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    if (data.size() < preface.size()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    std::string_view data_view{reinterpret_cast<const char*>(data.data()), preface.size()};
    if (data_view == preface) {
        return preface.size();
    }
    
    return std::unexpected(error_code::protocol_error);
}

} // namespace co::http::v2