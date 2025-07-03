#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

#if __cplusplus >= 202002L
#include <concepts>
#include <expected>
#include <span>
#else
#include <system_error>
namespace std {
    template<typename T, typename E>
    class expected {
    public:
        using value_type = T;
        using error_type = E;
        
        expected(T value) : has_value_(true), value_(std::move(value)) {}
        expected(std::unexpected<E> error) : has_value_(false), error_(std::move(error.error())) {}
        
        bool has_value() const noexcept { return has_value_; }
        operator bool() const noexcept { return has_value_; }
        
        T& operator*() & { return value_; }
        const T& operator*() const & { return value_; }
        T&& operator*() && { return std::move(value_); }
        
        E& error() & { return error_; }
        const E& error() const & { return error_; }
        E&& error() && { return std::move(error_); }
        
    private:
        bool has_value_;
        union {
            T value_;
            E error_;
        };
    };
    
    template<typename E>
    class unexpected {
    public:
        explicit unexpected(E error) : error_(std::move(error)) {}
        E& error() & { return error_; }
        const E& error() const & { return error_; }
        E&& error() && { return std::move(error_); }
    private:
        E error_;
    };
    
    template<typename T>
    class span {
    public:
        using element_type = T;
        using size_type = size_t;
        
        span() = default;
        span(T* data, size_type size) : data_(data), size_(size) {}
        
        T* data() const noexcept { return data_; }
        size_type size() const noexcept { return size_; }
        bool empty() const noexcept { return size_ == 0; }
        
        T& operator[](size_type idx) const { return data_[idx]; }
        
        span subspan(size_type offset) const {
            return span(data_ + offset, size_ - offset);
        }
        
    private:
        T* data_ = nullptr;
        size_type size_ = 0;
    };
}
#endif

namespace co::http::v1 {

enum class error_code {
    success = 0,
    need_more_data,
    invalid_method,
    invalid_uri,
    invalid_version,
    invalid_header,
    invalid_body,
    message_too_large,
    bad_request
};

enum class method {
    get,
    post,
    put,
    delete_,
    head,
    options,
    trace,
    connect,
    patch,
    unknown
};

struct header {
    std::string name;
    std::string value;
};

struct request {
    method method_val;
    std::string uri;
    std::string version;
    std::vector<header> headers;
    std::string body;
    
    std::optional<std::string_view> get_header(std::string_view name) const noexcept;
    void add_header(std::string name, std::string value);
};

struct response {
    std::string version;
    unsigned int status_code;
    std::string reason_phrase;
    std::vector<header> headers;
    std::string body;
    
    std::optional<std::string_view> get_header(std::string_view name) const noexcept;
    void add_header(std::string name, std::string value);
};

class parser {
public:
    parser() = default;
    
    std::expected<size_t, error_code> parse_request(std::span<const char> data, request& req);
    std::expected<size_t, error_code> parse_response(std::span<const char> data, response& resp);
    
    void reset() noexcept;
    bool is_complete() const noexcept { return complete_; }
    
private:
    enum class state {
        start,
        method,
        uri,
        version,
        header_name,
        header_value,
        body,
        complete
    };
    
    state state_ = state::start;
    size_t parsed_bytes_ = 0;
    bool complete_ = false;
    size_t content_length_ = 0;
    bool chunked_ = false;
    
    std::expected<size_t, error_code> parse_request_line(std::span<const char> data, request& req);
    std::expected<size_t, error_code> parse_status_line(std::span<const char> data, response& resp);
    std::expected<size_t, error_code> parse_headers(std::span<const char> data, 
                                                   std::vector<header>& headers);
    std::expected<size_t, error_code> parse_body(std::span<const char> data, std::string& body);
    
    method parse_method(std::string_view method_str) const noexcept;
    std::optional<size_t> find_line_end(std::span<const char> data) const noexcept;
    bool is_valid_header_name(std::string_view name) const noexcept;
    bool is_valid_header_value(std::string_view value) const noexcept;
};

#if __cplusplus >= 202002L
template<typename T>
concept http_message = requires(T t) {
    { t.headers } -> std::convertible_to<std::vector<header>&>;
    { t.body } -> std::convertible_to<std::string&>;
    { t.get_header(std::string_view{}) } -> std::convertible_to<std::optional<std::string_view>>;
    { t.add_header(std::string{}, std::string{}) } -> std::same_as<void>;
};

static_assert(http_message<request>);
static_assert(http_message<response>);
#endif

} // namespace co::http::v1