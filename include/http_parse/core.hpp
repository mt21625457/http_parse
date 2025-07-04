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

enum class status_code {
    // 1xx Informational
    continue_ = 100,
    switching_protocols = 101,
    processing = 102,
    
    // 2xx Success
    ok = 200,
    created = 201,
    accepted = 202,
    non_authoritative_information = 203,
    no_content = 204,
    reset_content = 205,
    partial_content = 206,
    
    // 3xx Redirection
    multiple_choices = 300,
    moved_permanently = 301,
    found = 302,
    see_other = 303,
    not_modified = 304,
    use_proxy = 305,
    temporary_redirect = 307,
    permanent_redirect = 308,
    
    // 4xx Client Error
    bad_request = 400,
    unauthorized = 401,
    payment_required = 402,
    forbidden = 403,
    not_found = 404,
    method_not_allowed = 405,
    not_acceptable = 406,
    proxy_authentication_required = 407,
    request_timeout = 408,
    conflict = 409,
    gone = 410,
    length_required = 411,
    precondition_failed = 412,
    payload_too_large = 413,
    uri_too_long = 414,
    unsupported_media_type = 415,
    range_not_satisfiable = 416,
    expectation_failed = 417,
    im_a_teapot = 418,
    unprocessable_entity = 422,
    upgrade_required = 426,
    precondition_required = 428,
    too_many_requests = 429,
    request_header_fields_too_large = 431,
    
    // 5xx Server Error
    internal_server_error = 500,
    not_implemented = 501,
    bad_gateway = 502,
    service_unavailable = 503,
    gateway_timeout = 504,
    http_version_not_supported = 505,
    insufficient_storage = 507,
    loop_detected = 508,
    not_extended = 510,
    network_authentication_required = 511
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