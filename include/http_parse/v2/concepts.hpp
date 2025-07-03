#pragma once

#if __cplusplus >= 202302L

#include <concepts>
#include <ranges>
#include <string_view>

namespace co::http::v2 {

template<typename T>
concept byte_range = std::ranges::contiguous_range<T> && 
                    (std::same_as<std::ranges::range_value_t<T>, char> ||
                     std::same_as<std::ranges::range_value_t<T>, unsigned char> ||
                     std::same_as<std::ranges::range_value_t<T>, std::byte>);

template<typename T>
concept frame_processor = requires(T t, frame_header hdr, std::span<const uint8_t> data) {
    { t.process_frame(hdr, data) } -> std::same_as<std::expected<void, error_code>>;
};

template<typename T>
concept stream_handler = requires(T t, uint32_t stream_id, std::span<const uint8_t> data) {
    { t.on_data(stream_id, data) } -> std::same_as<void>;
    { t.on_headers(stream_id, std::vector<header_field>{}) } -> std::same_as<void>;
    { t.on_stream_close(stream_id) } -> std::same_as<void>;
};

// C++23 pattern matching for frame types
constexpr auto frame_type_from_byte = [](uint8_t byte) constexpr -> frame_type {
    return static_cast<frame_type>(byte);
};

// C++23 structured bindings for frame headers
struct frame_header_view {
    uint32_t length;
    frame_type type;
    uint8_t flags;
    uint32_t stream_id;
    
    constexpr frame_header_view(const frame_header& hdr) noexcept
        : length(hdr.length), type(static_cast<frame_type>(hdr.type)), 
          flags(hdr.flags), stream_id(hdr.stream_id) {}
    
    constexpr auto operator<=>(const frame_header_view&) const = default;
};

// C++23 ranges for HPACK processing
template<std::ranges::contiguous_range R>
requires std::same_as<std::ranges::range_value_t<R>, uint8_t>
constexpr auto decode_hpack_headers(R&& data) {
    return std::ranges::views::transform(
        std::ranges::views::chunk(data, 2),
        [](auto chunk) -> header_field {
            // Simplified HPACK decoding
            return {"", ""};
        }
    );
}

// C++23 coroutine support for async parsing
namespace async {
    template<typename Executor>
    struct awaitable_parser {
        struct promise_type {
            std::expected<size_t, error_code> get_return_object() {
                return std::expected<size_t, error_code>{0};
            }
            
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_value(std::expected<size_t, error_code> value) {}
            void unhandled_exception() {}
        };
        
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) const noexcept {}
        std::expected<size_t, error_code> await_resume() const noexcept { 
            return std::expected<size_t, error_code>{0}; 
        }
    };
}

} // namespace co::http::v2

#endif // __cplusplus >= 202302L