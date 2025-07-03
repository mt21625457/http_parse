#pragma once

#if __cplusplus >= 202302L

#include <concepts>
#include <ranges>
#include <string_view>

namespace co::http::v1 {

template<typename T>
concept byte_range = std::ranges::contiguous_range<T> && 
                    std::same_as<std::ranges::range_value_t<T>, char> ||
                    std::same_as<std::ranges::range_value_t<T>, unsigned char> ||
                    std::same_as<std::ranges::range_value_t<T>, std::byte>;

template<typename T>
concept http_message_like = requires(T t) {
    { t.headers } -> std::ranges::range;
    { t.body } -> std::convertible_to<std::string>;
    { t.get_header(std::string_view{}) } -> std::same_as<std::optional<std::string_view>>;
};

template<typename Parser, typename Message>
concept http_parser = requires(Parser p, Message& msg, std::span<const char> data) {
    { p.parse(data, msg) } -> std::same_as<std::expected<size_t, error_code>>;
    { p.reset() } -> std::same_as<void>;
    { p.is_complete() } -> std::same_as<bool>;
};

// C++23 pattern matching for HTTP methods
constexpr auto method_from_string = []<std::size_t N>(const char (&str)[N]) constexpr -> method {
    std::string_view sv{str, N-1};
    
    if (sv == "GET") return method::get;
    if (sv == "POST") return method::post;
    if (sv == "PUT") return method::put;
    if (sv == "DELETE") return method::delete_;
    if (sv == "HEAD") return method::head;
    if (sv == "OPTIONS") return method::options;
    if (sv == "TRACE") return method::trace;
    if (sv == "CONNECT") return method::connect;
    if (sv == "PATCH") return method::patch;
    
    return method::unknown;
};

// C++23 string literal operators
namespace literals {
    consteval method operator""_method(const char* str, std::size_t len) {
        std::string_view sv{str, len};
        
        if (sv == "GET") return method::get;
        if (sv == "POST") return method::post;
        if (sv == "PUT") return method::put;
        if (sv == "DELETE") return method::delete_;
        if (sv == "HEAD") return method::head;
        if (sv == "OPTIONS") return method::options;
        if (sv == "TRACE") return method::trace;
        if (sv == "CONNECT") return method::connect;
        if (sv == "PATCH") return method::patch;
        
        return method::unknown;
    }
}

// C++23 multidimensional subscript operator for header access
template<http_message_like T>
class header_accessor {
public:
    explicit header_accessor(T& msg) : msg_(msg) {}
    
    std::optional<std::string_view> operator[](std::string_view name) const {
        return msg_.get_header(name);
    }
    
    void operator[](std::string_view name, std::string_view value) {
        msg_.add_header(std::string{name}, std::string{value});
    }
    
private:
    T& msg_;
};

} // namespace co::http::v1

#endif // __cplusplus >= 202302L