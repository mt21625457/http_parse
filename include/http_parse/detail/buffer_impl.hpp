#pragma once

#include "../buffer.hpp"
#include <algorithm>

namespace co::http {

// =============================================================================
// Output Buffer Implementation
// =============================================================================

inline output_buffer::output_buffer(size_t initial_capacity) {
    buffer_.reserve(initial_capacity);
}

inline void output_buffer::append(std::string_view data) {
    buffer_.append(data);
}

inline void output_buffer::append(std::span<const uint8_t> data) {
    buffer_.append(reinterpret_cast<const char*>(data.data()), data.size());
}

inline void output_buffer::append(const char* data, size_t size) {
    buffer_.append(data, size);
}

inline void output_buffer::append_byte(uint8_t byte) {
    buffer_.push_back(static_cast<char>(byte));
}

inline void output_buffer::reserve(size_t capacity) {
    buffer_.reserve(capacity);
}

inline std::span<const uint8_t> output_buffer::data() const noexcept {
    return std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>(buffer_.data()), 
        buffer_.size()
    };
}

inline std::string_view output_buffer::string_view() const noexcept {
    return std::string_view{buffer_};
}

inline size_t output_buffer::size() const noexcept {
    return buffer_.size();
}

inline bool output_buffer::empty() const noexcept {
    return buffer_.empty();
}

inline void output_buffer::clear() noexcept {
    buffer_.clear();
}

inline std::string output_buffer::release_string() {
    return std::move(buffer_);
}

inline std::string output_buffer::to_string() const {
    return buffer_;
}

inline std::string_view output_buffer::view() const noexcept {
    return std::string_view{buffer_};
}

inline std::span<const uint8_t> output_buffer::span() const noexcept {
    return std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>(buffer_.data()), 
        buffer_.size()
    };
}

} // namespace co::http