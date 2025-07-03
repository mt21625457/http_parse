#pragma once

#include "../core.hpp"

namespace co::http::v1 {

// =============================================================================
// HTTP/1.x Parser Interface
// =============================================================================

class parser {
public:
    parser() = default;
    ~parser() = default;
    
    // Non-copyable, movable
    parser(const parser&) = delete;
    parser& operator=(const parser&) = delete;
    parser(parser&&) = default;
    parser& operator=(parser&&) = default;
    
    // Parse complete messages
    std::expected<request, error_code> parse_request(std::string_view data);
    std::expected<response, error_code> parse_response(std::string_view data);
    
    // Incremental parsing
    std::expected<size_t, error_code> parse_request_incremental(std::span<const char> data, request& req);
    std::expected<size_t, error_code> parse_response_incremental(std::span<const char> data, response& resp);
    
    // Parsing state
    bool is_parse_complete() const noexcept;
    bool needs_more_data() const noexcept;
    version detected_version() const noexcept;
    void reset() noexcept;
    
private:
    // Implementation details
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
    
    void reset_state();
};

} // namespace co::http::v1

// Include implementation
#include "parser_impl.hpp"