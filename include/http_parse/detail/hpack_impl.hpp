#pragma once

#include "hpack.hpp"
#include <algorithm>
#include <cctype>

namespace co::http::detail {

// =============================================================================
// HPACK Encoder Implementation
// =============================================================================

inline std::expected<size_t, error_code> hpack_encoder::encode_headers(const std::vector<header>& headers, output_buffer& output) {
    size_t initial_size = output.size();
    
    try {
        for (const auto& hdr : headers) {
            // Check if header exists in static or dynamic table
            auto index = find_header_index(hdr.name, hdr.value);
            if (index) {
                // Indexed Header Field Representation
                encode_indexed_header(*index, output);
            } else {
                // Check if name exists in table
                auto name_index = find_name_index(hdr.name);
                
                if (hdr.sensitive) {
                    // Never Indexed Literal Header Field
                    encode_literal_header_never_indexed(hdr.name, hdr.value, output);
                } else {
                    // Literal Header Field with Incremental Indexing
                    encode_literal_header_with_indexing(hdr.name, hdr.value, output);
                    add_to_dynamic_table(hdr.name, hdr.value);
                }
            }
        }
        
        return output.size() - initial_size;
    } catch (...) {
        return std::unexpected(error_code::compression_error);
    }
}

inline void hpack_encoder::set_dynamic_table_size(uint32_t size) {
    dynamic_table_size_ = size;
    while (dynamic_table_current_size_ > dynamic_table_size_) {
        evict_from_dynamic_table();
    }
}

inline void hpack_encoder::clear_dynamic_table() {
    dynamic_table_.clear();
    dynamic_table_current_size_ = 0;
}

inline size_t hpack_encoder::encode_indexed_header(size_t index, output_buffer& output) {
    // Indexed Header Field: 1xxxxxxx
    return encode_integer(static_cast<uint32_t>(index), 7, 0x80, output);
}

inline size_t hpack_encoder::encode_literal_header_with_indexing(std::string_view name, std::string_view value, output_buffer& output) {
    size_t initial_size = output.size();
    
    // Literal Header Field with Incremental Indexing: 01xxxxxx
    auto name_index = find_name_index(name);
    if (name_index) {
        encode_integer(static_cast<uint32_t>(*name_index), 6, 0x40, output);
    } else {
        encode_integer(0, 6, 0x40, output);
        encode_string(name, output);
    }
    encode_string(value, output);
    
    return output.size() - initial_size;
}

inline size_t hpack_encoder::encode_literal_header_never_indexed(std::string_view name, std::string_view value, output_buffer& output) {
    size_t initial_size = output.size();
    
    // Literal Header Field Never Indexed: 0001xxxx
    auto name_index = find_name_index(name);
    if (name_index) {
        encode_integer(static_cast<uint32_t>(*name_index), 4, 0x10, output);
    } else {
        encode_integer(0, 4, 0x10, output);
        encode_string(name, output);
    }
    encode_string(value, output);
    
    return output.size() - initial_size;
}

inline size_t hpack_encoder::encode_literal_header_without_indexing(std::string_view name, std::string_view value, output_buffer& output) {
    size_t initial_size = output.size();
    
    // Literal Header Field without Indexing: 0000xxxx
    auto name_index = find_name_index(name);
    if (name_index) {
        encode_integer(static_cast<uint32_t>(*name_index), 4, 0x00, output);
    } else {
        encode_integer(0, 4, 0x00, output);
        encode_string(name, output);
    }
    encode_string(value, output);
    
    return output.size() - initial_size;
}

inline size_t hpack_encoder::encode_dynamic_table_size_update(uint32_t size, output_buffer& output) {
    // Dynamic Table Size Update: 001xxxxx
    return encode_integer(size, 5, 0x20, output);
}

inline std::optional<size_t> hpack_encoder::find_header_index(std::string_view name, std::string_view value) const {
    // Search static table
    for (size_t i = 0; i < static_table_.size(); ++i) {
        if (static_table_[i].first == name && static_table_[i].second == value) {
            return i + 1; // HPACK indices are 1-based
        }
    }
    
    // Search dynamic table
    for (size_t i = 0; i < dynamic_table_.size(); ++i) {
        if (dynamic_table_[i].name == name && dynamic_table_[i].value == value) {
            return static_table_.size() + 1 + i; // Dynamic table starts after static table
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
            return static_table_.size() + 1 + i;
        }
    }
    
    return std::nullopt;
}

inline void hpack_encoder::add_to_dynamic_table(std::string_view name, std::string_view value) {
    dynamic_entry entry{std::string(name), std::string(value)};
    size_t entry_size = entry.size();
    
    // Check if entry is larger than table size
    if (entry_size > dynamic_table_size_) {
        clear_dynamic_table();
        return;
    }
    
    // Evict entries if necessary
    while (dynamic_table_current_size_ + entry_size > dynamic_table_size_) {
        evict_from_dynamic_table();
    }
    
    // Add new entry at the beginning
    dynamic_table_.insert(dynamic_table_.begin(), std::move(entry));
    dynamic_table_current_size_ += entry_size;
}

inline void hpack_encoder::evict_from_dynamic_table() {
    if (!dynamic_table_.empty()) {
        dynamic_table_current_size_ -= dynamic_table_.back().size();
        dynamic_table_.pop_back();
    }
}

inline size_t hpack_encoder::encode_string(std::string_view str, output_buffer& output, bool huffman) {
    size_t initial_size = output.size();
    
    if (huffman) {
        // Huffman encoded string
        uint8_t first_byte = 0x80; // H=1
        encode_integer(static_cast<uint32_t>(str.size()), 7, first_byte, output); // Simplified: assume no compression
        output.append(str);
    } else {
        // Plain string
        encode_integer(static_cast<uint32_t>(str.size()), 7, 0x00, output);
        output.append(str);
    }
    
    return output.size() - initial_size;
}

inline size_t hpack_encoder::encode_integer(uint32_t value, uint8_t prefix_bits, uint8_t first_byte, output_buffer& output) {
    size_t initial_size = output.size();
    uint32_t max_prefix = (1U << prefix_bits) - 1;
    
    if (value < max_prefix) {
        // Value fits in prefix
        uint8_t byte = first_byte | static_cast<uint8_t>(value);
        output.append(reinterpret_cast<const char*>(&byte), 1);
    } else {
        // Value requires multiple bytes
        uint8_t first = first_byte | static_cast<uint8_t>(max_prefix);
        output.append(reinterpret_cast<const char*>(&first), 1);
        
        value -= max_prefix;
        while (value >= 128) {
            uint8_t byte = static_cast<uint8_t>((value % 128) + 128);
            output.append(reinterpret_cast<const char*>(&byte), 1);
            value /= 128;
        }
        
        uint8_t last = static_cast<uint8_t>(value);
        output.append(reinterpret_cast<const char*>(&last), 1);
    }
    
    return output.size() - initial_size;
}

// =============================================================================
// HPACK Decoder Implementation
// =============================================================================

inline std::expected<std::vector<header>, error_code> hpack_decoder::decode_headers(std::span<const uint8_t> data) {
    std::vector<header> headers;
    size_t pos = 0;
    
    try {
        while (pos < data.size()) {
            uint8_t first_byte = data[pos];
            
            if (first_byte & 0x80) {
                // Indexed Header Field
                header hdr;
                auto consumed = decode_indexed_header(data, pos, hdr);
                if (!consumed) return std::unexpected(consumed.error());
                headers.push_back(std::move(hdr));
            } else if (first_byte & 0x40) {
                // Literal Header Field with Incremental Indexing
                header hdr;
                auto consumed = decode_literal_header(data, pos, hdr, true, false);
                if (!consumed) return std::unexpected(consumed.error());
                headers.push_back(hdr);
                add_to_dynamic_table(hdr.name, hdr.value);
            } else if (first_byte & 0x20) {
                // Dynamic Table Size Update
                auto consumed = decode_dynamic_table_size_update(data, pos);
                if (!consumed) return std::unexpected(consumed.error());
            } else if (first_byte & 0x10) {
                // Literal Header Field Never Indexed
                header hdr;
                auto consumed = decode_literal_header(data, pos, hdr, false, true);
                if (!consumed) return std::unexpected(consumed.error());
                hdr.sensitive = true;
                headers.push_back(std::move(hdr));
            } else {
                // Literal Header Field without Indexing
                header hdr;
                auto consumed = decode_literal_header(data, pos, hdr, false, false);
                if (!consumed) return std::unexpected(consumed.error());
                headers.push_back(std::move(hdr));
            }
        }
        
        return headers;
    } catch (...) {
        return std::unexpected(error_code::compression_error);
    }
}

inline void hpack_decoder::set_dynamic_table_size(uint32_t size) {
    dynamic_table_size_ = size;
    while (dynamic_table_current_size_ > dynamic_table_size_) {
        evict_from_dynamic_table();
    }
}

inline void hpack_decoder::clear_dynamic_table() {
    dynamic_table_.clear();
    dynamic_table_current_size_ = 0;
}

inline std::expected<size_t, error_code> hpack_decoder::decode_indexed_header(std::span<const uint8_t> data, size_t& pos, header& result) {
    auto index = decode_integer(data, pos, 7);
    if (!index) return std::unexpected(index.error());
    
    auto header_pair = get_header_by_index(*index);
    if (!header_pair) return std::unexpected(header_pair.error());
    
    result.name = std::string(header_pair->first);
    result.value = std::string(header_pair->second);
    
    return pos;
}

inline std::expected<size_t, error_code> hpack_decoder::decode_literal_header(std::span<const uint8_t> data, size_t& pos, header& result, bool with_indexing, bool never_indexed) {
    uint8_t prefix_bits = with_indexing ? 6 : (never_indexed ? 4 : 4);
    
    auto name_index = decode_integer(data, pos, prefix_bits);
    if (!name_index) return std::unexpected(name_index.error());
    
    if (*name_index == 0) {
        // Name is encoded as string
        auto name = decode_string(data, pos);
        if (!name) return std::unexpected(name.error());
        result.name = std::move(*name);
    } else {
        // Name is indexed
        auto name = get_name_by_index(*name_index);
        if (!name) return std::unexpected(name.error());
        result.name = std::string(*name);
    }
    
    // Value is always encoded as string
    auto value = decode_string(data, pos);
    if (!value) return std::unexpected(value.error());
    result.value = std::move(*value);
    
    return pos;
}

inline std::expected<size_t, error_code> hpack_decoder::decode_dynamic_table_size_update(std::span<const uint8_t> data, size_t& pos) {
    auto size = decode_integer(data, pos, 5);
    if (!size) return std::unexpected(size.error());
    
    set_dynamic_table_size(*size);
    return pos;
}

inline std::expected<std::pair<std::string_view, std::string_view>, error_code> hpack_decoder::get_header_by_index(size_t index) const {
    if (index == 0) {
        return std::unexpected(error_code::compression_error);
    }
    
    if (index <= static_table_.size()) {
        return static_table_[index - 1];
    }
    
    size_t dynamic_index = index - static_table_.size() - 1;
    if (dynamic_index >= dynamic_table_.size()) {
        return std::unexpected(error_code::compression_error);
    }
    
    const auto& entry = dynamic_table_[dynamic_index];
    return std::make_pair(std::string_view(entry.name), std::string_view(entry.value));
}

inline std::expected<std::string_view, error_code> hpack_decoder::get_name_by_index(size_t index) const {
    if (index == 0) {
        return std::unexpected(error_code::compression_error);
    }
    
    if (index <= static_table_.size()) {
        return static_table_[index - 1].first;
    }
    
    size_t dynamic_index = index - static_table_.size() - 1;
    if (dynamic_index >= dynamic_table_.size()) {
        return std::unexpected(error_code::compression_error);
    }
    
    return std::string_view(dynamic_table_[dynamic_index].name);
}

inline void hpack_decoder::add_to_dynamic_table(std::string_view name, std::string_view value) {
    dynamic_entry entry{std::string(name), std::string(value)};
    size_t entry_size = entry.size();
    
    if (entry_size > dynamic_table_size_) {
        clear_dynamic_table();
        return;
    }
    
    while (dynamic_table_current_size_ + entry_size > dynamic_table_size_) {
        evict_from_dynamic_table();
    }
    
    dynamic_table_.insert(dynamic_table_.begin(), std::move(entry));
    dynamic_table_current_size_ += entry_size;
}

inline void hpack_decoder::evict_from_dynamic_table() {
    if (!dynamic_table_.empty()) {
        dynamic_table_current_size_ -= dynamic_table_.back().size();
        dynamic_table_.pop_back();
    }
}

inline std::expected<std::string, error_code> hpack_decoder::decode_string(std::span<const uint8_t> data, size_t& pos) {
    if (pos >= data.size()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    bool huffman_encoded = (data[pos] & 0x80) != 0;
    auto length = decode_integer(data, pos, 7);
    if (!length) return std::unexpected(length.error());
    
    if (pos + *length > data.size()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    std::string result;
    if (huffman_encoded) {
        // Simplified: no Huffman decoding for now
        result = std::string(reinterpret_cast<const char*>(data.data() + pos), *length);
    } else {
        result = std::string(reinterpret_cast<const char*>(data.data() + pos), *length);
    }
    
    pos += *length;
    return result;
}

inline std::expected<uint32_t, error_code> hpack_decoder::decode_integer(std::span<const uint8_t> data, size_t& pos, uint8_t prefix_bits) {
    if (pos >= data.size()) {
        return std::unexpected(error_code::need_more_data);
    }
    
    uint32_t max_prefix = (1U << prefix_bits) - 1;
    uint32_t value = data[pos] & max_prefix;
    pos++;
    
    if (value < max_prefix) {
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
            return std::unexpected(error_code::compression_error);
        }
    }
    
    return std::unexpected(error_code::need_more_data);
}

} // namespace co::http::detail