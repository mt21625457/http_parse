#pragma once

#include "v1/parser.hpp"
#include "v1/parser_impl.hpp"
#include "v2/parser.hpp"
#include "v2/parser_impl.hpp"

#include <variant>
#include <memory>
#include <string_view>

namespace co::http {

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
    unsupported_version,
    parse_error
};

struct message {
    version protocol_version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    std::optional<std::string_view> get_header(std::string_view name) const noexcept;
    void add_header(std::string name, std::string value);
};

struct request : message {
    std::string method;
    std::string uri;
    std::string target;
};

struct response : message {
    unsigned int status_code;
    std::string reason_phrase;
};

class parser {
public:
    explicit parser(version ver = version::auto_detect);
    
    std::expected<size_t, error_code> parse_request(std::span<const char> data, request& req);
    std::expected<size_t, error_code> parse_response(std::span<const char> data, response& resp);
    
    void reset() noexcept;
    bool is_complete() const noexcept;
    version detected_version() const noexcept { return detected_version_; }
    
    // HTTP/2 specific callbacks
    void set_on_headers(std::function<void(uint32_t, const std::vector<v2::header_field>&)> callback);
    void set_on_data(std::function<void(uint32_t, std::span<const uint8_t>)> callback);
    void set_on_stream_error(std::function<void(uint32_t, v2::error_code)> callback);
    
private:
    version version_;
    version detected_version_ = version::http_1_1;
    
    std::variant<v1::parser, v2::parser> parser_impl_;
    
    version detect_version(std::span<const char> data) const;
    error_code convert_v1_error(v1::error_code error) const;
    error_code convert_v2_error(v2::error_code error) const;
    
    request convert_v1_request(const v1::request& req) const;
    response convert_v1_response(const v1::response& resp) const;
    request convert_v2_request(uint32_t stream_id, const std::vector<v2::header_field>& headers, 
                              const std::string& body) const;
    response convert_v2_response(uint32_t stream_id, const std::vector<v2::header_field>& headers,
                               const std::string& body) const;
};

// Convenience functions
inline std::expected<request, error_code> parse_request(std::string_view data, version ver = version::auto_detect) {
    parser p(ver);
    request req;
    auto result = p.parse_request(std::span<const char>(data.data(), data.size()), req);
    if (result) {
        return req;
    }
    return result.error();
}

inline std::expected<response, error_code> parse_response(std::string_view data, version ver = version::auto_detect) {
    parser p(ver);
    response resp;
    auto result = p.parse_response(std::span<const char>(data.data(), data.size()), resp);
    if (result) {
        return resp;
    }
    return result.error();
}

// HTTP/2 specific functions
namespace v2 {
    using stream_id = uint32_t;
    
    class connection {
    public:
        connection() = default;
        
        std::expected<size_t, error_code> process_data(std::span<const uint8_t> data);
        
        void set_on_headers(std::function<void(stream_id, const std::vector<header_field>&)> callback);
        void set_on_data(std::function<void(stream_id, std::span<const uint8_t>)> callback);
        void set_on_stream_error(std::function<void(stream_id, v2::error_code)> callback);
        void set_on_connection_error(std::function<void(v2::error_code)> callback);
        
        std::vector<stream_id> active_streams() const;
        std::optional<std::string> get_stream_data(stream_id id) const;
        std::optional<std::vector<header_field>> get_stream_headers(stream_id id) const;
        
    private:
        parser parser_;
        std::unordered_map<stream_id, std::pair<std::vector<header_field>, std::string>> streams_;
    };
}

} // namespace co::http