#pragma once

#include <string>
#include <string_view>
#include <span>
#include <cstdint>

namespace co::http {

// =============================================================================
// High-Performance Buffer Interface
// =============================================================================

class output_buffer {
public:
    output_buffer() = default;
    explicit output_buffer(size_t initial_capacity);
    
    // Non-copyable, movable
    output_buffer(const output_buffer&) = delete;
    output_buffer& operator=(const output_buffer&) = delete;
    output_buffer(output_buffer&&) = default;
    output_buffer& operator=(output_buffer&&) = default;
    
    // Append data
    void append(std::string_view data);
    void append(std::span<const uint8_t> data);
    void append(const char* data, size_t size);
    void append_byte(uint8_t byte);
    
    // Reserve capacity
    void reserve(size_t capacity);
    
    // Access data
    std::span<const uint8_t> data() const noexcept;
    std::string_view string_view() const noexcept;
    size_t size() const noexcept;
    bool empty() const noexcept;
    
    // Clear buffer
    void clear() noexcept;
    
    // Transfer ownership
    std::string release_string();
    
    // Convert to string
    std::string to_string() const;
    
    // View methods
    std::string_view view() const noexcept;
    std::span<const uint8_t> span() const noexcept;
    
private:
    std::string buffer_;
};

} // namespace co::http

// Include implementation
#include "detail/buffer_impl.hpp"