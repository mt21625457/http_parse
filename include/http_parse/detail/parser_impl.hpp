#pragma once

#include "../parser.hpp"
#include "../core.hpp"
#include "hpack_impl.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <charconv>

namespace co::http {

// =============================================================================
// Parser Implementation Details
// =============================================================================

class parser::impl {
public:
    enum class parse_state {
        method,
        uri,
        version,
        header_name,
        header_value,
        body,
        complete,
        error
    };

    version version_ = version::auto_detect;
    parse_state state_ = parse_state::method;
    version detected_version_ = version::http_1_1;
    bool parse_complete_ = false;
    bool needs_more_data_ = false;
    
    // HTTP/1.x parsing state
    std::string current_header_name_;
    size_t content_length_ = 0;
    size_t body_bytes_read_ = 0;
    bool chunked_encoding_ = false;
    bool connection_close_ = false;
    
    // HTTP/2 callbacks
    parser::stream_request_callback stream_request_cb_;
    parser::stream_response_callback stream_response_cb_;
    parser::stream_data_callback stream_data_cb_;
    parser::stream_error_callback stream_error_cb_;
    parser::connection_error_callback connection_error_cb_;
    parser::settings_callback settings_cb_;
    parser::ping_callback ping_cb_;
    parser::goaway_callback goaway_cb_;
    
    // HTTP/2 settings
    uint32_t max_frame_size_ = 16384;
    uint32_t max_header_list_size_ = 8192;
    
    // HPACK decoder
    detail::hpack_decoder hpack_decoder_;
    
    impl(version ver) : version_(ver) {}
    
    void reset() {
        state_ = parse_state::method;
        parse_complete_ = false;
        needs_more_data_ = false;
        current_header_name_.clear();
        content_length_ = 0;
        body_bytes_read_ = 0;
        chunked_encoding_ = false;
        connection_close_ = false;
    }
};

// =============================================================================
// Parser Public Interface Implementation
// =============================================================================

inline parser::parser(version ver) : pimpl_(std::make_unique<impl>(ver)) {}

inline parser::~parser() = default;

inline parser::parser(parser&&) noexcept = default;
inline parser& parser::operator=(parser&&) noexcept = default;

inline bool parser::is_parse_complete() const noexcept {
    return pimpl_->parse_complete_;
}

inline bool parser::needs_more_data() const noexcept {
    return pimpl_->needs_more_data_;
}

inline version parser::detected_version() const noexcept {
    return pimpl_->detected_version_;
}

inline void parser::reset() noexcept {
    pimpl_->reset();
}

// =============================================================================
// HTTP/1.x Parsing Implementation
// =============================================================================

namespace detail {

inline std::pair<std::string_view, std::string_view> split_first(std::string_view str, char delimiter) {
    auto pos = str.find(delimiter);
    if (pos == std::string_view::npos) {
        return {str, {}};
    }
    return {str.substr(0, pos), str.substr(pos + 1)};
}

inline std::string_view trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

inline bool iequals(std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char ca, char cb) { return std::tolower(ca) == std::tolower(cb); });
}

inline std::expected<request, error_code> parse_http1_request(std::string_view data) {
    request req;
    
    // Find first line (request line)
    auto line_end = data.find("\r\n");
    if (line_end == std::string_view::npos) {
        return std::unexpected(error_code::need_more_data);
    }
    
    auto request_line = data.substr(0, line_end);
    
    // Parse method
    auto [method_str, rest1] = split_first(request_line, ' ');
    if (rest1.empty()) {
        return std::unexpected(error_code::invalid_method);
    }
    req.set_method(method_str);
    
    // Parse URI
    auto [uri_str, version_str] = split_first(rest1, ' ');
    if (version_str.empty()) {
        return std::unexpected(error_code::invalid_uri);
    }
    req.uri = uri_str;
    req.target = uri_str; // For HTTP/2 compatibility
    
    // Parse version
    if (version_str == "HTTP/1.0") {
        req.protocol_version = version::http_1_0;
    } else if (version_str == "HTTP/1.1") {
        req.protocol_version = version::http_1_1;
    } else {
        return std::unexpected(error_code::invalid_version);
    }
    
    // Parse headers
    size_t pos = line_end + 2;
    while (pos < data.size()) {
        auto header_line_end = data.find("\r\n", pos);
        if (header_line_end == std::string_view::npos) {
            return std::unexpected(error_code::need_more_data);
        }
        
        auto header_line = data.substr(pos, header_line_end - pos);
        if (header_line.empty()) {
            // Empty line indicates end of headers
            pos = header_line_end + 2;
            break;
        }
        
        auto [name, value] = split_first(header_line, ':');
        if (value.empty()) {
            return std::unexpected(error_code::invalid_header);
        }
        
        req.add_header(std::string(trim(name)), std::string(trim(value)));
        pos = header_line_end + 2;
    }
    
    // Parse body if present
    if (pos < data.size()) {
        auto content_length_hdr = req.get_header("content-length");
        if (content_length_hdr) {
            size_t content_length;
            auto result = std::from_chars(content_length_hdr->data(), 
                                        content_length_hdr->data() + content_length_hdr->size(), 
                                        content_length);
            if (result.ec == std::errc{}) {
                if (data.size() - pos >= content_length) {
                    req.body = std::string(data.substr(pos, content_length));
                } else {
                    return std::unexpected(error_code::need_more_data);
                }
            }
        } else {
            // No content-length, assume rest is body
            req.body = std::string(data.substr(pos));
        }
    }
    
    return req;
}

inline std::expected<response, error_code> parse_http1_response(std::string_view data) {
    response resp;
    
    // Find first line (status line)
    auto line_end = data.find("\r\n");
    if (line_end == std::string_view::npos) {
        return std::unexpected(error_code::need_more_data);
    }
    
    auto status_line = data.substr(0, line_end);
    
    // Parse version
    auto [version_str, rest1] = split_first(status_line, ' ');
    if (rest1.empty()) {
        return std::unexpected(error_code::invalid_version);
    }
    
    if (version_str == "HTTP/1.0") {
        resp.protocol_version = version::http_1_0;
    } else if (version_str == "HTTP/1.1") {
        resp.protocol_version = version::http_1_1;
    } else {
        return std::unexpected(error_code::invalid_version);
    }
    
    // Parse status code
    auto [status_str, reason_str] = split_first(rest1, ' ');
    auto result = std::from_chars(status_str.data(), status_str.data() + status_str.size(), resp.status_code);
    if (result.ec != std::errc{}) {
        return std::unexpected(error_code::protocol_error);
    }
    
    if (!reason_str.empty()) {
        resp.reason_phrase = reason_str;
    }
    
    // Parse headers and body (same logic as request)
    size_t pos = line_end + 2;
    while (pos < data.size()) {
        auto header_line_end = data.find("\r\n", pos);
        if (header_line_end == std::string_view::npos) {
            return std::unexpected(error_code::need_more_data);
        }
        
        auto header_line = data.substr(pos, header_line_end - pos);
        if (header_line.empty()) {
            pos = header_line_end + 2;
            break;
        }
        
        auto [name, value] = split_first(header_line, ':');
        if (value.empty()) {
            return std::unexpected(error_code::invalid_header);
        }
        
        resp.add_header(std::string(trim(name)), std::string(trim(value)));
        pos = header_line_end + 2;
    }
    
    // Parse body
    if (pos < data.size()) {
        auto content_length_hdr = resp.get_header("content-length");
        if (content_length_hdr) {
            size_t content_length;
            auto result = std::from_chars(content_length_hdr->data(), 
                                        content_length_hdr->data() + content_length_hdr->size(), 
                                        content_length);
            if (result.ec == std::errc{}) {
                if (data.size() - pos >= content_length) {
                    resp.body = std::string(data.substr(pos, content_length));
                } else {
                    return std::unexpected(error_code::need_more_data);
                }
            }
        } else {
            resp.body = std::string(data.substr(pos));
        }
    }
    
    return resp;
}

// =============================================================================
// HTTP/2 Header Conversion Functions
// =============================================================================

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

inline std::expected<request, error_code> parser::parse_request(std::string_view data) {
    return detail::parse_http1_request(data);
}

inline std::expected<response, error_code> parser::parse_response(std::string_view data) {
    return detail::parse_http1_response(data);
}

inline std::expected<size_t, error_code> parser::parse_request_incremental(std::span<const char> data, request& req) {
    std::string_view data_view{data.data(), data.size()};
    auto result = detail::parse_http1_request(data_view);
    if (result) {
        req = std::move(*result);
        pimpl_->parse_complete_ = true;
        return data.size();
    } else {
        if (result.error() == error_code::need_more_data) {
            pimpl_->needs_more_data_ = true;
        }
        return std::unexpected(result.error());
    }
}

inline std::expected<size_t, error_code> parser::parse_response_incremental(std::span<const char> data, response& resp) {
    std::string_view data_view{data.data(), data.size()};
    auto result = detail::parse_http1_response(data_view);
    if (result) {
        resp = std::move(*result);
        pimpl_->parse_complete_ = true;
        return data.size();
    } else {
        if (result.error() == error_code::need_more_data) {
            pimpl_->needs_more_data_ = true;
        }
        return std::unexpected(result.error());
    }
}

// =============================================================================
// HTTP/2 Interface Implementation
// =============================================================================

inline void parser::set_stream_request_callback(stream_request_callback callback) {
    pimpl_->stream_request_cb_ = std::move(callback);
}

inline void parser::set_stream_response_callback(stream_response_callback callback) {
    pimpl_->stream_response_cb_ = std::move(callback);
}

inline void parser::set_stream_data_callback(stream_data_callback callback) {
    pimpl_->stream_data_cb_ = std::move(callback);
}

inline void parser::set_stream_error_callback(stream_error_callback callback) {
    pimpl_->stream_error_cb_ = std::move(callback);
}

inline void parser::set_connection_error_callback(connection_error_callback callback) {
    pimpl_->connection_error_cb_ = std::move(callback);
}

inline void parser::set_settings_callback(settings_callback callback) {
    pimpl_->settings_cb_ = std::move(callback);
}

inline void parser::set_ping_callback(ping_callback callback) {
    pimpl_->ping_cb_ = std::move(callback);
}

inline void parser::set_goaway_callback(goaway_callback callback) {
    pimpl_->goaway_cb_ = std::move(callback);
}

inline std::expected<size_t, error_code> parser::parse_h2_frames(std::span<const uint8_t> data) {
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
                if (pimpl_->stream_data_cb_) {
                    bool end_stream = (flags & 0x01) != 0;
                    pimpl_->stream_data_cb_(stream_id, payload, end_stream);
                }
                break;
                
            case 0x01: // HEADERS frame
                try {
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
                        
                        if (is_request && pimpl_->stream_request_cb_) {
                            request req = detail::convert_h2_headers_to_request(*headers);
                            pimpl_->stream_request_cb_(stream_id, req);
                        } else if (!is_request && pimpl_->stream_response_cb_) {
                            response resp = detail::convert_h2_headers_to_response(*headers);
                            pimpl_->stream_response_cb_(stream_id, resp);
                        }
                    } else if (pimpl_->stream_error_cb_) {
                        pimpl_->stream_error_cb_(stream_id, headers.error());
                    }
                } catch (...) {
                    if (pimpl_->stream_error_cb_) {
                        pimpl_->stream_error_cb_(stream_id, error_code::compression_error);
                    }
                }
                break;
                
            case 0x04: // SETTINGS frame
                if (pimpl_->settings_cb_) {
                    std::unordered_map<uint16_t, uint32_t> settings;
                    for (size_t i = 0; i + 6 <= payload.size(); i += 6) {
                        uint16_t id = (static_cast<uint16_t>(payload[i]) << 8) | payload[i + 1];
                        uint32_t value = (static_cast<uint32_t>(payload[i + 2]) << 24) |
                                        (static_cast<uint32_t>(payload[i + 3]) << 16) |
                                        (static_cast<uint32_t>(payload[i + 4]) << 8) |
                                        static_cast<uint32_t>(payload[i + 5]);
                        settings[id] = value;
                    }
                    pimpl_->settings_cb_(settings);
                }
                break;
                
            case 0x06: // PING frame
                if (pimpl_->ping_cb_ && payload.size() == 8) {
                    bool ack = (flags & 0x01) != 0;
                    std::span<const uint8_t, 8> ping_data{payload.data(), 8};
                    pimpl_->ping_cb_(ping_data, ack);
                }
                break;
                
            case 0x07: // GOAWAY frame
                if (pimpl_->goaway_cb_ && payload.size() >= 8) {
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
                    
                    pimpl_->goaway_cb_(last_stream_id, static_cast<error_code>(error_code_val), debug_data);
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

inline std::expected<size_t, error_code> parser::parse_h2_preface(std::span<const uint8_t> data) {
    // HTTP/2 connection preface: "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
    const std::string_view preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    if (data.size() < preface.size()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    std::string_view data_view{reinterpret_cast<const char*>(data.data()), preface.size()};
    if (data_view == preface) {
        pimpl_->detected_version_ = version::http_2_0;
        return preface.size();
    }
    
    return std::unexpected(error_code::protocol_error);
}

inline void parser::set_h2_max_frame_size(uint32_t size) {
    pimpl_->max_frame_size_ = size;
}

inline void parser::set_h2_max_header_list_size(uint32_t size) {
    pimpl_->max_header_list_size_ = size;
}

inline uint32_t parser::get_h2_max_frame_size() const noexcept {
    return pimpl_->max_frame_size_;
}

inline uint32_t parser::get_h2_max_header_list_size() const noexcept {
    return pimpl_->max_header_list_size_;
}

} // namespace co::http