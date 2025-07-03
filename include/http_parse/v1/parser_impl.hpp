#pragma once

#include "parser.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>

namespace co::http::v1 {

inline std::optional<std::string_view> request::get_header(std::string_view name) const noexcept {
    auto it = std::find_if(headers.begin(), headers.end(), 
        [name](const header& h) { 
            return std::equal(h.name.begin(), h.name.end(), name.begin(), name.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        });
    return it != headers.end() ? std::make_optional(std::string_view{it->value}) : std::nullopt;
}

inline void request::add_header(std::string name, std::string value) {
    headers.emplace_back(std::move(name), std::move(value));
}

inline std::optional<std::string_view> response::get_header(std::string_view name) const noexcept {
    auto it = std::find_if(headers.begin(), headers.end(), 
        [name](const header& h) { 
            return std::equal(h.name.begin(), h.name.end(), name.begin(), name.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        });
    return it != headers.end() ? std::make_optional(std::string_view{it->value}) : std::nullopt;
}

inline void response::add_header(std::string name, std::string value) {
    headers.emplace_back(std::move(name), std::move(value));
}

inline std::expected<size_t, error_code> parser::parse_request(std::span<const char> data, request& req) {
    if (data.empty()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    size_t consumed = 0;
    
    while (consumed < data.size() && !complete_) {
        auto remaining = data.subspan(consumed);
        
        switch (state_) {
            case state::start:
            case state::method:
            case state::uri:
            case state::version: {
                auto result = parse_request_line(remaining, req);
                if (!result) return result;
                consumed += *result;
                if (state_ == state::version) {
                    state_ = state::header_name;
                }
                break;
            }
            
            case state::header_name:
            case state::header_value: {
                auto result = parse_headers(remaining, req.headers);
                if (!result) return result;
                consumed += *result;
                if (state_ == state::body) {
                    auto content_len = req.get_header("content-length");
                    if (content_len) {
                        std::from_chars(content_len->data(), content_len->data() + content_len->size(), content_length_);
                    }
                    
                    auto transfer_encoding = req.get_header("transfer-encoding");
                    if (transfer_encoding && transfer_encoding->find("chunked") != std::string_view::npos) {
                        chunked_ = true;
                    }
                }
                break;
            }
            
            case state::body: {
                auto result = parse_body(remaining, req.body);
                if (!result) return result;
                consumed += *result;
                break;
            }
            
            case state::complete:
                complete_ = true;
                break;
        }
    }
    
    return consumed;
}

inline std::expected<size_t, error_code> parser::parse_response(std::span<const char> data, response& resp) {
    if (data.empty()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    size_t consumed = 0;
    
    while (consumed < data.size() && !complete_) {
        auto remaining = data.subspan(consumed);
        
        switch (state_) {
            case state::start: {
                auto result = parse_status_line(remaining, resp);
                if (!result) return result;
                consumed += *result;
                state_ = state::header_name;
                break;
            }
            
            case state::header_name:
            case state::header_value: {
                auto result = parse_headers(remaining, resp.headers);
                if (!result) return result;
                consumed += *result;
                if (state_ == state::body) {
                    auto content_len = resp.get_header("content-length");
                    if (content_len) {
                        std::from_chars(content_len->data(), content_len->data() + content_len->size(), content_length_);
                    }
                    
                    auto transfer_encoding = resp.get_header("transfer-encoding");
                    if (transfer_encoding && transfer_encoding->find("chunked") != std::string_view::npos) {
                        chunked_ = true;
                    }
                }
                break;
            }
            
            case state::body: {
                auto result = parse_body(remaining, resp.body);
                if (!result) return result;
                consumed += *result;
                break;
            }
            
            case state::complete:
                complete_ = true;
                break;
        }
    }
    
    return consumed;
}

inline void parser::reset() noexcept {
    state_ = state::start;
    parsed_bytes_ = 0;
    complete_ = false;
    content_length_ = 0;
    chunked_ = false;
}

inline std::expected<size_t, error_code> parser::parse_request_line(std::span<const char> data, request& req) {
    auto line_end = find_line_end(data);
    if (!line_end) {
        return std::unexpected(error_code::need_more_data);
    }
    
    std::string_view line{data.data(), *line_end};
    
    // Parse method
    auto method_end = line.find(' ');
    if (method_end == std::string_view::npos) {
        return std::unexpected(error_code::invalid_method);
    }
    
    req.method_val = parse_method(line.substr(0, method_end));
    if (req.method_val == method::unknown) {
        return std::unexpected(error_code::invalid_method);
    }
    
    // Parse URI
    auto uri_start = method_end + 1;
    auto uri_end = line.find(' ', uri_start);
    if (uri_end == std::string_view::npos) {
        return std::unexpected(error_code::invalid_uri);
    }
    
    req.uri = line.substr(uri_start, uri_end - uri_start);
    
    // Parse version
    auto version_start = uri_end + 1;
    req.version = line.substr(version_start);
    
    if (!req.version.starts_with("HTTP/")) {
        return std::unexpected(error_code::invalid_version);
    }
    
    return *line_end + 2; // +2 for CRLF
}

inline std::expected<size_t, error_code> parser::parse_status_line(std::span<const char> data, response& resp) {
    auto line_end = find_line_end(data);
    if (!line_end) {
        return std::unexpected(error_code::need_more_data);
    }
    
    std::string_view line{data.data(), *line_end};
    
    // Parse version
    auto version_end = line.find(' ');
    if (version_end == std::string_view::npos) {
        return std::unexpected(error_code::invalid_version);
    }
    
    resp.version = line.substr(0, version_end);
    
    // Parse status code
    auto status_start = version_end + 1;
    auto status_end = line.find(' ', status_start);
    if (status_end == std::string_view::npos) {
        return std::unexpected(error_code::bad_request);
    }
    
    auto status_str = line.substr(status_start, status_end - status_start);
    auto [ptr, ec] = std::from_chars(status_str.data(), status_str.data() + status_str.size(), resp.status_code);
    if (ec != std::errc{}) {
        return std::unexpected(error_code::bad_request);
    }
    
    // Parse reason phrase
    auto reason_start = status_end + 1;
    resp.reason_phrase = line.substr(reason_start);
    
    return *line_end + 2; // +2 for CRLF
}

inline method parser::parse_method(std::string_view method_str) const noexcept {
    if (method_str == "GET") return method::get;
    if (method_str == "POST") return method::post;
    if (method_str == "PUT") return method::put;
    if (method_str == "DELETE") return method::delete_;
    if (method_str == "HEAD") return method::head;
    if (method_str == "OPTIONS") return method::options;
    if (method_str == "TRACE") return method::trace;
    if (method_str == "CONNECT") return method::connect;
    if (method_str == "PATCH") return method::patch;
    return method::unknown;
}

inline std::optional<size_t> parser::find_line_end(std::span<const char> data) const noexcept {
    for (size_t i = 0; i < data.size() - 1; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace co::http::v1