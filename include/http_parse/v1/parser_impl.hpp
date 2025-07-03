#pragma once

#include "parser.hpp"
#include "../detail/core_impl.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <charconv>

namespace co::http::v1 {

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

} // namespace detail

// =============================================================================
// Public Interface Implementation
// =============================================================================

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
        parse_complete_ = true;
        detected_version_ = req.protocol_version;
        return data.size();
    } else {
        if (result.error() == error_code::need_more_data) {
            needs_more_data_ = true;
        }
        return std::unexpected(result.error());
    }
}

inline std::expected<size_t, error_code> parser::parse_response_incremental(std::span<const char> data, response& resp) {
    std::string_view data_view{data.data(), data.size()};
    auto result = detail::parse_http1_response(data_view);
    if (result) {
        resp = std::move(*result);
        parse_complete_ = true;
        detected_version_ = resp.protocol_version;
        return data.size();
    } else {
        if (result.error() == error_code::need_more_data) {
            needs_more_data_ = true;
        }
        return std::unexpected(result.error());
    }
}

inline bool parser::is_parse_complete() const noexcept {
    return parse_complete_;
}

inline bool parser::needs_more_data() const noexcept {
    return needs_more_data_;
}

inline version parser::detected_version() const noexcept {
    return detected_version_;
}

inline void parser::reset() noexcept {
    reset_state();
}

inline void parser::reset_state() {
    state_ = parse_state::method;
    parse_complete_ = false;
    needs_more_data_ = false;
    current_header_name_.clear();
    content_length_ = 0;
    body_bytes_read_ = 0;
    chunked_encoding_ = false;
    connection_close_ = false;
}

} // namespace co::http::v1