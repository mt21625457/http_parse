#pragma once

#include <string>
#include <string_view>
#include <span>
#include <expected>
#include <vector>
#include <optional>
#include <cstdint>

namespace co::http {

// =============================================================================
// Core Types and Enums
// =============================================================================

enum class version {
    http_1_0,
    http_1_1, 
    http_2_0,
    auto_detect
};

enum class error_code {
    success = 0,
    need_more_data,
    protocol_error,
    invalid_method,
    invalid_uri,
    invalid_version,
    invalid_header,
    invalid_body,
    frame_size_error,
    compression_error,
    flow_control_error,
    stream_closed,
    connection_error
};

enum class method {
    get, post, put, delete_, head, options, trace, connect, patch, unknown
};

// Header representation
struct header {
    std::string name;
    std::string value;
    bool sensitive = false;  // For HTTP/2 HPACK never-indexed headers
    
    header() = default;
    header(std::string n, std::string v, bool s = false) 
        : name(std::move(n)), value(std::move(v)), sensitive(s) {}
};

// HTTP message base
struct message {
    version protocol_version = version::http_1_1;
    std::vector<header> headers;
    std::string body;
    
    std::optional<std::string_view> get_header(std::string_view name) const noexcept;
    void add_header(std::string name, std::string value, bool sensitive = false);
    void set_header(std::string name, std::string value, bool sensitive = false);
    bool has_header(std::string_view name) const noexcept;
    void remove_header(std::string_view name);
};

// HTTP request
struct request : message {
    method method_type = method::get;
    std::string uri;
    std::string target; // For HTTP/2 :path pseudo-header
    
    void set_method(method m) { method_type = m; }
    void set_method(std::string_view m);
    std::string get_method_string() const;
};

// HTTP response  
struct response : message {
    unsigned int status_code = 200;
    std::string reason_phrase = "OK";
};

} // namespace co::http

// Include implementation
#include "detail/core_impl.hpp"