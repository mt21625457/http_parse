#pragma once

#include "hpack.hpp"
#include <algorithm>

namespace co::http::detail {

// =============================================================================
// HPACK Encoder Implementation
// =============================================================================

inline std::expected<size_t, error_code> hpack_encoder::encode_headers(const std::vector<header>& headers, output_buffer& output) {
    size_t initial_size = output.size();
    
    for (const auto& h : headers) {
        // Check if header is in static or dynamic table
        if (auto index = find_header_index(h.name, h.value)) {
            // Encode as indexed header field
            encode_indexed_header(*index, output);
        } else if (auto name_index = find_name_index(h.name)) {
            // Encode as literal header field with incremental indexing (name indexed)
            if (h.sensitive) {
                encode_literal_header_never_indexed(h.name, h.value, output);
            } else {
                encode_literal_header_with_indexing(h.name, h.value, output);
                add_to_dynamic_table(h.name, h.value);
            }
        } else {
            // Encode as literal header field with incremental indexing (new name)
            if (h.sensitive) {
                encode_literal_header_never_indexed(h.name, h.value, output);
            } else {
                encode_literal_header_with_indexing(h.name, h.value, output);
                add_to_dynamic_table(h.name, h.value);
            }
        }
    }
    
    return output.size() - initial_size;
}

inline size_t hpack_encoder::encode_indexed_header(size_t index, output_buffer& output) {
    // Pattern: 1xxxxxxx
    return encode_integer(static_cast<uint32_t>(index), 7, 0x80, output);
}

inline size_t hpack_encoder::encode_literal_header_with_indexing(std::string_view name, std::string_view value, output_buffer& output) {
    size_t bytes_written = 0;
    
    // Pattern: 01xxxxxx
    if (auto name_index = find_name_index(name)) {
        bytes_written += encode_integer(static_cast<uint32_t>(*name_index), 6, 0x40, output);
    } else {
        bytes_written += encode_integer(0, 6, 0x40, output);
        bytes_written += encode_string(name, output);
    }
    
    bytes_written += encode_string(value, output);
    return bytes_written;
}

inline size_t hpack_encoder::encode_literal_header_never_indexed(std::string_view name, std::string_view value, output_buffer& output) {
    size_t bytes_written = 0;
    
    // Pattern: 0001xxxx
    if (auto name_index = find_name_index(name)) {
        bytes_written += encode_integer(static_cast<uint32_t>(*name_index), 4, 0x10, output);
    } else {
        bytes_written += encode_integer(0, 4, 0x10, output);
        bytes_written += encode_string(name, output);
    }
    
    bytes_written += encode_string(value, output);
    return bytes_written;
}

inline size_t hpack_encoder::encode_literal_header_without_indexing(std::string_view name, std::string_view value, output_buffer& output) {
    size_t bytes_written = 0;
    
    // Pattern: 0000xxxx
    if (auto name_index = find_name_index(name)) {
        bytes_written += encode_integer(static_cast<uint32_t>(*name_index), 4, 0x00, output);
    } else {
        bytes_written += encode_integer(0, 4, 0x00, output);
        bytes_written += encode_string(name, output);
    }
    
    bytes_written += encode_string(value, output);
    return bytes_written;
}

inline size_t hpack_encoder::encode_string(std::string_view str, output_buffer& output, bool huffman) {
    size_t bytes_written = 0;
    
    if (huffman) {
        // Simplified Huffman encoding - would use proper table in production
        bytes_written += encode_integer(static_cast<uint32_t>(str.size()), 7, 0x80, output);
        output.append(str);
        bytes_written += str.size();
    } else {
        // Encode without Huffman
        bytes_written += encode_integer(static_cast<uint32_t>(str.size()), 7, 0x00, output);
        output.append(str);
        bytes_written += str.size();
    }
    
    return bytes_written;
}

inline size_t hpack_encoder::encode_integer(uint32_t value, uint8_t prefix_bits, uint8_t first_byte, output_buffer& output) {
    uint32_t prefix_max = (1U << prefix_bits) - 1;
    
    if (value < prefix_max) {
        output.append_byte(first_byte | static_cast<uint8_t>(value));
        return 1;
    }
    
    output.append_byte(first_byte | static_cast<uint8_t>(prefix_max));
    value -= prefix_max;
    
    size_t bytes_written = 1;
    while (value >= 128) {
        output.append_byte(static_cast<uint8_t>((value % 128) + 128));
        value /= 128;
        bytes_written++;
    }
    
    output.append_byte(static_cast<uint8_t>(value));
    return bytes_written + 1;
}

inline std::optional<size_t> hpack_encoder::find_header_index(std::string_view name, std::string_view value) const {
    // Search static table
    for (size_t i = 0; i < static_table_.size(); ++i) {
        if (static_table_[i].first == name && static_table_[i].second == value) {
            return i + 1; // 1-based indexing
        }
    }
    
    // Search dynamic table
    for (size_t i = 0; i < dynamic_table_.size(); ++i) {
        if (dynamic_table_[i].name == name && dynamic_table_[i].value == value) {
            return static_table_.size() + i + 1;
        }
    }
    
    return std::nullopt;
}

inline std::optional<size_t> hpack_encoder::find_name_index(std::string_view name) const {
    // Search static table
    for (size_t i = 0; i < static_table_.size(); ++i) {
        if (static_table_[i].first == name) {
            return i + 1;
        }
    }
    
    // Search dynamic table
    for (size_t i = 0; i < dynamic_table_.size(); ++i) {
        if (dynamic_table_[i].name == name) {
            return static_table_.size() + i + 1;
        }
    }
    
    return std::nullopt;
}

inline void hpack_encoder::add_to_dynamic_table(std::string_view name, std::string_view value) {
    dynamic_entry entry{std::string(name), std::string(value)};
    size_t entry_size = entry.size();
    
    // Evict entries if necessary
    while (dynamic_table_current_size_ + entry_size > dynamic_table_size_ && !dynamic_table_.empty()) {
        evict_from_dynamic_table();
    }
    
    if (entry_size <= dynamic_table_size_) {
        dynamic_table_.insert(dynamic_table_.begin(), std::move(entry));
        dynamic_table_current_size_ += entry_size;
    }
}

inline void hpack_encoder::evict_from_dynamic_table() {
    if (!dynamic_table_.empty()) {
        dynamic_table_current_size_ -= dynamic_table_.back().size();
        dynamic_table_.pop_back();
    }
}

inline void hpack_encoder::set_dynamic_table_size(uint32_t size) {
    dynamic_table_size_ = size;
    
    // Evict entries if new size is smaller
    while (dynamic_table_current_size_ > dynamic_table_size_ && !dynamic_table_.empty()) {
        evict_from_dynamic_table();
    }
}

inline void hpack_encoder::clear_dynamic_table() {
    dynamic_table_.clear();
    dynamic_table_current_size_ = 0;
}

// =============================================================================
// HPACK Decoder Implementation
// =============================================================================

inline std::expected<std::vector<header>, error_code> hpack_decoder::decode_headers(std::span<const uint8_t> data) {
    std::vector<header> headers;
    size_t pos = 0;
    
    while (pos < data.size()) {
        uint8_t first_byte = data[pos];
        
        if (first_byte & 0x80) {
            // Indexed Header Field (1xxxxxxx)
            header h;
            auto result = decode_indexed_header(data, pos, h);
            if (!result) {
                return std::unexpected{result.error()};
            }
            headers.push_back(std::move(h));
        } else if (first_byte & 0x40) {
            // Literal Header Field with Incremental Indexing (01xxxxxx)
            header h;
            auto result = decode_literal_header(data, pos, h, true, false);
            if (!result) {
                return std::unexpected{result.error()};
            }
            headers.push_back(std::move(h));
        } else if (first_byte & 0x20) {
            // Dynamic Table Size Update (001xxxxx)
            auto result = decode_dynamic_table_size_update(data, pos);
            if (!result) {
                return std::unexpected{result.error()};
            }
        } else if (first_byte & 0x10) {
            // Literal Header Field Never Indexed (0001xxxx)
            header h;
            auto result = decode_literal_header(data, pos, h, false, true);
            if (!result) {
                return std::unexpected{result.error()};
            }
            h.sensitive = true;
            headers.push_back(std::move(h));
        } else {
            // Literal Header Field without Indexing (0000xxxx)
            header h;
            auto result = decode_literal_header(data, pos, h, false, false);
            if (!result) {
                return std::unexpected{result.error()};
            }
            headers.push_back(std::move(h));
        }
    }
    
    return headers;
}

inline std::expected<size_t, error_code> hpack_decoder::decode_indexed_header(std::span<const uint8_t> data, size_t& pos, header& result) {
    auto index_result = decode_integer(data, pos, 7);
    if (!index_result) {
        return std::unexpected{index_result.error()};
    }
    
    auto header_result = get_header_by_index(*index_result);
    if (!header_result) {
        return std::unexpected{header_result.error()};
    }
    
    result.name = std::string(header_result->first);
    result.value = std::string(header_result->second);
    
    return pos;
}

inline std::expected<size_t, error_code> hpack_decoder::decode_literal_header(std::span<const uint8_t> data, size_t& pos, header& result, bool with_indexing, bool never_indexed) {
    uint8_t prefix_bits = with_indexing ? 6 : (never_indexed ? 4 : 4);
    
    auto name_index_result = decode_integer(data, pos, prefix_bits);
    if (!name_index_result) {
        return std::unexpected{name_index_result.error()};
    }
    
    if (*name_index_result == 0) {
        // Name is literal
        auto name_result = decode_string(data, pos);
        if (!name_result) {
            return std::unexpected{name_result.error()};
        }
        result.name = std::move(*name_result);
    } else {
        // Name is indexed
        auto name_result = get_name_by_index(*name_index_result);
        if (!name_result) {
            return std::unexpected{name_result.error()};
        }
        result.name = std::string(*name_result);
    }
    
    // Decode value
    auto value_result = decode_string(data, pos);
    if (!value_result) {
        return std::unexpected{value_result.error()};
    }
    result.value = std::move(*value_result);
    
    // Add to dynamic table if with_indexing
    if (with_indexing && !never_indexed) {
        add_to_dynamic_table(result.name, result.value);
    }
    
    return pos;
}

inline std::expected<uint32_t, error_code> hpack_decoder::decode_integer(std::span<const uint8_t> data, size_t& pos, uint8_t prefix_bits) {
    if (pos >= data.size()) {
        return std::unexpected{error_code::need_more_data};
    }
    
    uint32_t prefix_max = (1U << prefix_bits) - 1;
    uint32_t value = data[pos++] & prefix_max;
    
    if (value < prefix_max) {
        return value;
    }
    
    uint32_t m = 0;
    while (pos < data.size()) {
        uint8_t byte = data[pos++];
        value += (byte & 0x7F) << m;
        
        if ((byte & 0x80) == 0) {
            return value;
        }
        
        m += 7;
        if (m >= 32) {
            return std::unexpected{error_code::protocol_error};
        }
    }
    
    return std::unexpected{error_code::need_more_data};
}

inline std::expected<std::string, error_code> hpack_decoder::decode_string(std::span<const uint8_t> data, size_t& pos) {
    if (pos >= data.size()) {
        return std::unexpected{error_code::need_more_data};
    }
    
    bool huffman_encoded = (data[pos] & 0x80) != 0;
    
    auto length_result = decode_integer(data, pos, 7);
    if (!length_result) {
        return std::unexpected{length_result.error()};
    }
    
    uint32_t length = *length_result;
    if (pos + length > data.size()) {
        return std::unexpected{error_code::need_more_data};
    }
    
    std::string result;
    
    if (huffman_encoded) {
        // Simplified - real implementation would decode Huffman
        result = std::string(reinterpret_cast<const char*>(data.data() + pos), length);
    } else {
        result = std::string(reinterpret_cast<const char*>(data.data() + pos), length);
    }
    
    pos += length;
    return result;
}

inline std::expected<std::pair<std::string_view, std::string_view>, error_code> hpack_decoder::get_header_by_index(size_t index) const {
    if (index == 0) {
        return std::unexpected{error_code::protocol_error};
    }
    
    if (index <= static_table_.size()) {
        return static_table_[index - 1];
    }
    
    size_t dynamic_index = index - static_table_.size() - 1;
    if (dynamic_index >= dynamic_table_.size()) {
        return std::unexpected{error_code::protocol_error};
    }
    
    const auto& entry = dynamic_table_[dynamic_index];
    return std::pair<std::string_view, std::string_view>{entry.name, entry.value};
}

inline std::expected<std::string_view, error_code> hpack_decoder::get_name_by_index(size_t index) const {
    auto header_result = get_header_by_index(index);
    if (!header_result) {
        return std::unexpected{header_result.error()};
    }
    return header_result->first;
}

inline void hpack_decoder::add_to_dynamic_table(std::string_view name, std::string_view value) {
    dynamic_entry entry{std::string(name), std::string(value)};
    size_t entry_size = entry.size();
    
    // Evict entries if necessary
    while (dynamic_table_current_size_ + entry_size > dynamic_table_size_ && !dynamic_table_.empty()) {
        evict_from_dynamic_table();
    }
    
    if (entry_size <= dynamic_table_size_) {
        dynamic_table_.insert(dynamic_table_.begin(), std::move(entry));
        dynamic_table_current_size_ += entry_size;
    }
}

inline void hpack_decoder::evict_from_dynamic_table() {
    if (!dynamic_table_.empty()) {
        dynamic_table_current_size_ -= dynamic_table_.back().size();
        dynamic_table_.pop_back();
    }
}

inline void hpack_decoder::set_dynamic_table_size(uint32_t size) {
    dynamic_table_size_ = size;
    
    // Evict entries if new size is smaller
    while (dynamic_table_current_size_ > dynamic_table_size_ && !dynamic_table_.empty()) {
        evict_from_dynamic_table();
    }
}

inline void hpack_decoder::clear_dynamic_table() {
    dynamic_table_.clear();
    dynamic_table_current_size_ = 0;
}

inline std::expected<size_t, error_code> hpack_decoder::decode_dynamic_table_size_update(std::span<const uint8_t> data, size_t& pos) {
    auto size_result = decode_integer(data, pos, 5);
    if (!size_result) {
        return std::unexpected{size_result.error()};
    }
    
    set_dynamic_table_size(*size_result);
    return pos;
}

} // namespace co::http::detail