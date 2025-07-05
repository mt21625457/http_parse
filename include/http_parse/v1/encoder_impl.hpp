#pragma once

#include "encoder.hpp"
#include "../detail/core_impl.hpp"
#include <sstream>
#include <algorithm>

namespace co::http::v1 {

// =============================================================================
// HTTP/1.x Encoding Implementation
// =============================================================================

namespace detail {

inline std::string encode_http1_request(const request& req, version ver) {
    std::ostringstream oss;
    
    // Request line
    std::string path = req.target.empty() ? req.uri : req.target;
    oss << req.get_method_string() << " " << path << " ";
    switch (ver) {
        case version::http_1_0: oss << "HTTP/1.0"; break;
        case version::http_1_1: oss << "HTTP/1.1"; break;
        default: oss << "HTTP/1.1"; break;
    }
    oss << "\r\n";
    
    // Headers
    for (const auto& header : req.headers) {
        std::string lower_name = header.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c){ return std::tolower(c); });
        oss << lower_name << ": " << header.value << "\r\n";
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

inline std::string encode_http1_response(const response& resp, version ver) {
    std::ostringstream oss;
    
    // Status line
    switch (ver) {
        case version::http_1_0: oss << "HTTP/1.0"; break;
        case version::http_1_1: oss << "HTTP/1.1"; break;
        default: oss << "HTTP/1.1"; break;
    }
    oss << " " << resp.status_code << " " << resp.reason_phrase << "\r\n";
    
    // Headers
    for (const auto& header : resp.headers) {
        std::string lower_name = header.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c){ return std::tolower(c); });
        oss << lower_name << ": " << header.value << "\r\n";
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

inline size_t encode_http1_request_to_buffer(const request& req, output_buffer& output, version ver) {
    auto encoded = encode_http1_request(req, ver);
    size_t size = encoded.size();
    output.append(encoded);
    return size;
}

inline size_t encode_http1_response_to_buffer(const response& resp, output_buffer& output, version ver) {
    auto encoded = encode_http1_response(resp, ver);
    size_t size = encoded.size();
    output.append(encoded);
    return size;
}

} // namespace detail

// =============================================================================
// Public Interface Implementation
// =============================================================================

inline std::expected<std::string, error_code> encoder::encode_request(const request& req) {
    try {
        return detail::encode_http1_request(req, version_);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<std::string, error_code> encoder::encode_response(const response& resp) {
    try {
        return detail::encode_http1_response(resp, version_);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_request(const request& req, output_buffer& output) {
    try {
        return detail::encode_http1_request_to_buffer(req, output, version_);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

inline std::expected<size_t, error_code> encoder::encode_response(const response& resp, output_buffer& output) {
    try {
        return detail::encode_http1_response_to_buffer(resp, output, version_);
    } catch (...) {
        return std::unexpected(error_code::protocol_error);
    }
}

} // namespace co::http::v1