#pragma once

#include "../core.hpp"
#include <algorithm>
#include <cctype>

namespace co::http {

// =============================================================================
// Message Implementation
// =============================================================================

inline std::optional<std::string_view> message::get_header(std::string_view name) const noexcept {
    auto it = std::find_if(headers.begin(), headers.end(), 
        [name](const header& h) {
            return std::equal(name.begin(), name.end(), h.name.begin(), h.name.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        });
    
    if (it != headers.end()) {
        return std::string_view{it->value};
    }
    return std::nullopt;
}

inline void message::add_header(std::string name, std::string value, bool sensitive) {
    headers.emplace_back(std::move(name), std::move(value), sensitive);
}

inline void message::set_header(std::string name, std::string value, bool sensitive) {
    auto it = std::find_if(headers.begin(), headers.end(),
        [&name](const header& h) {
            return std::equal(name.begin(), name.end(), h.name.begin(), h.name.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        });
    
    if (it != headers.end()) {
        it->value = std::move(value);
        it->sensitive = sensitive;
    } else {
        add_header(std::move(name), std::move(value), sensitive);
    }
}

inline bool message::has_header(std::string_view name) const noexcept {
    return get_header(name).has_value();
}

inline void message::remove_header(std::string_view name) {
    headers.erase(
        std::remove_if(headers.begin(), headers.end(),
            [name](const header& h) {
                return std::equal(name.begin(), name.end(), h.name.begin(), h.name.end(),
                    [](char a, char b) { return std::tolower(a) == std::tolower(b); });
            }),
        headers.end());
}

// =============================================================================
// Request Implementation
// =============================================================================

inline void request::set_method(std::string_view m) {
    if (m == "GET") method_type = method::get;
    else if (m == "POST") method_type = method::post;
    else if (m == "PUT") method_type = method::put;
    else if (m == "DELETE") method_type = method::delete_;
    else if (m == "HEAD") method_type = method::head;
    else if (m == "OPTIONS") method_type = method::options;
    else if (m == "TRACE") method_type = method::trace;
    else if (m == "CONNECT") method_type = method::connect;
    else if (m == "PATCH") method_type = method::patch;
    else method_type = method::unknown;
}

inline std::string request::get_method_string() const {
    switch (method_type) {
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

// =============================================================================
// Utility Functions
// =============================================================================

inline std::string to_string(version v) {
    switch (v) {
        case version::http_1_0: return "HTTP/1.0";
        case version::http_1_1: return "HTTP/1.1";
        case version::http_2_0: return "HTTP/2.0";
        case version::auto_detect: return "AUTO";
    }
    return "UNKNOWN";
}

inline std::string to_string(error_code e) {
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

inline std::string to_string(method m) {
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

} // namespace co::http