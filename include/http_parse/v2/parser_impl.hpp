#pragma once

#include "parser.hpp"
#include <algorithm>
#include <cstring>
#include <functional>

namespace co::http::v2 {

static constexpr std::array<uint8_t, 24> connection_preface = {
    'P', 'R', 'I', ' ', '*', ' ', 'H', 'T', 'T', 'P', '/', '2', '.', '0', '\r', '\n',
    '\r', '\n', 'S', 'M', '\r', '\n', '\r', '\n'
};

const std::array<hpack_decoder::table_entry, 61> hpack_decoder::static_table_ = {{
    {"", ""},
    {":authority", ""},
    {":method", "GET"},
    {":method", "POST"},
    {":path", "/"},
    {":path", "/index.html"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "200"},
    {":status", "204"},
    {":status", "206"},
    {":status", "304"},
    {":status", "400"},
    {":status", "404"},
    {":status", "500"},
    {"accept-charset", ""},
    {"accept-encoding", "gzip, deflate"},
    {"accept-language", ""},
    {"accept-ranges", ""},
    {"accept", ""},
    {"access-control-allow-origin", ""},
    {"age", ""},
    {"allow", ""},
    {"authorization", ""},
    {"cache-control", ""},
    {"content-disposition", ""},
    {"content-encoding", ""},
    {"content-language", ""},
    {"content-length", ""},
    {"content-location", ""},
    {"content-range", ""},
    {"content-type", ""},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"expect", ""},
    {"expires", ""},
    {"from", ""},
    {"host", ""},
    {"if-match", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"if-range", ""},
    {"if-unmodified-since", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"max-forwards", ""},
    {"proxy-authenticate", ""},
    {"proxy-authorization", ""},
    {"range", ""},
    {"referer", ""},
    {"refresh", ""},
    {"retry-after", ""},
    {"server", ""},
    {"set-cookie", ""},
    {"strict-transport-security", ""},
    {"transfer-encoding", ""},
    {"user-agent", ""},
    {"vary", ""},
    {"via", ""},
    {"www-authenticate", ""}
}};

inline std::expected<size_t, error_code> parser::parse(std::span<const uint8_t> data) {
    if (data.empty()) {
        return error_code::need_more_data;
    }
    
    size_t consumed = 0;
    
    while (consumed < data.size()) {
        auto remaining = data.subspan(consumed);
        
        switch (state_) {
            case state::preface: {
                auto result = parse_preface(remaining);
                if (!result) return result;
                consumed += *result;
                if (preface_received_) {
                    state_ = state::frame_header;
                }
                break;
            }
            
            case state::frame_header: {
                auto result = parse_frame_header(remaining);
                if (!result) return result;
                consumed += *result;
                if (frame_bytes_ == frame_header::size) {
                    state_ = state::frame_payload;
                    frame_buffer_.resize(current_frame_.length);
                    frame_bytes_ = 0;
                }
                break;
            }
            
            case state::frame_payload: {
                auto result = parse_frame_payload(remaining);
                if (!result) return result;
                consumed += *result;
                if (frame_bytes_ == current_frame_.length) {
                    auto process_result = process_frame();
                    if (!process_result) {
                        return process_result.error();
                    }
                    state_ = state::frame_header;
                    frame_bytes_ = 0;
                    frame_complete_ = true;
                }
                break;
            }
            
            case state::complete:
                return consumed;
        }
    }
    
    return consumed;
}

inline std::expected<size_t, error_code> parser::parse_preface(std::span<const uint8_t> data) {
    size_t to_copy = std::min(data.size(), connection_preface.size() - preface_bytes_);
    
    if (std::memcmp(data.data(), connection_preface.data() + preface_bytes_, to_copy) != 0) {
        return error_code::protocol_error;
    }
    
    std::memcpy(preface_buffer_.data() + preface_bytes_, data.data(), to_copy);
    preface_bytes_ += to_copy;
    
    if (preface_bytes_ == connection_preface.size()) {
        preface_received_ = true;
    }
    
    return to_copy;
}

inline std::expected<size_t, error_code> parser::parse_frame_header(std::span<const uint8_t> data) {
    size_t to_copy = std::min(data.size(), frame_header::size - frame_bytes_);
    
    if (frame_bytes_ == 0) {
        if (data.size() < frame_header::size) {
            return error_code::need_more_data;
        }
        
        // Parse frame header
        current_frame_.length = (data[0] << 16) | (data[1] << 8) | data[2];
        current_frame_.type = data[3];
        current_frame_.flags = data[4];
        current_frame_.stream_id = ((data[5] & 0x7F) << 24) | (data[6] << 16) | (data[7] << 8) | data[8];
        current_frame_.reserved = (data[5] & 0x80) >> 7;
        
        if (current_frame_.length > max_frame_size_) {
            return error_code::frame_size_error;
        }
        
        frame_bytes_ = frame_header::size;
        return frame_header::size;
    }
    
    return 0;
}

inline std::expected<size_t, error_code> parser::parse_frame_payload(std::span<const uint8_t> data) {
    size_t to_copy = std::min(data.size(), current_frame_.length - frame_bytes_);
    
    if (to_copy > 0) {
        std::memcpy(frame_buffer_.data() + frame_bytes_, data.data(), to_copy);
        frame_bytes_ += to_copy;
    }
    
    return to_copy;
}

inline std::expected<void, error_code> parser::process_frame() {
    switch (static_cast<frame_type>(current_frame_.type)) {
        case frame_type::data:
            return process_data_frame();
        case frame_type::headers:
            return process_headers_frame();
        case frame_type::settings:
            return process_settings_frame();
        case frame_type::ping:
            return process_ping_frame();
        case frame_type::goaway:
            return process_goaway_frame();
        case frame_type::window_update:
            return process_window_update_frame();
        case frame_type::rst_stream:
            return process_rst_stream_frame();
        default:
            // Unknown frame type, ignore
            return {};
    }
}

inline std::expected<void, error_code> parser::process_data_frame() {
    if (current_frame_.stream_id == 0) {
        return error_code::protocol_error;
    }
    
    auto& stream = get_or_create_stream(current_frame_.stream_id);
    
    if (on_data) {
        on_data(current_frame_.stream_id, std::span<const uint8_t>(frame_buffer_));
    }
    
    stream.data.append(reinterpret_cast<const char*>(frame_buffer_.data()), frame_buffer_.size());
    
    if (current_frame_.flags & static_cast<uint8_t>(frame_flags::end_stream)) {
        stream.half_closed_remote = true;
    }
    
    return {};
}

inline std::expected<void, error_code> parser::process_headers_frame() {
    if (current_frame_.stream_id == 0) {
        return error_code::protocol_error;
    }
    
    auto& stream = get_or_create_stream(current_frame_.stream_id);
    
    auto headers_result = hpack_decoder_.decode(std::span<const uint8_t>(frame_buffer_));
    if (!headers_result) {
        return headers_result.error();
    }
    
    stream.headers = std::move(*headers_result);
    
    if (on_headers) {
        on_headers(current_frame_.stream_id, stream.headers);
    }
    
    if (current_frame_.flags & static_cast<uint8_t>(frame_flags::end_stream)) {
        stream.half_closed_remote = true;
    }
    
    return {};
}

inline std::expected<void, error_code> parser::process_settings_frame() {
    if (current_frame_.stream_id != 0) {
        return error_code::protocol_error;
    }
    
    if (current_frame_.flags & static_cast<uint8_t>(frame_flags::ack)) {
        return {};
    }
    
    settings_frame settings;
    
    for (size_t i = 0; i < frame_buffer_.size(); i += 6) {
        if (i + 6 > frame_buffer_.size()) {
            return error_code::frame_size_error;
        }
        
        uint16_t id = (frame_buffer_[i] << 8) | frame_buffer_[i + 1];
        uint32_t value = (frame_buffer_[i + 2] << 24) | (frame_buffer_[i + 3] << 16) | 
                        (frame_buffer_[i + 4] << 8) | frame_buffer_[i + 5];
        
        settings.settings[id] = value;
    }
    
    apply_settings(settings);
    
    if (on_settings) {
        on_settings(settings);
    }
    
    return {};
}

inline void parser::apply_settings(const settings_frame& settings) {
    for (const auto& [id, value] : settings.settings) {
        switch (id) {
            case settings_frame::header_table_size:
                header_table_size_ = value;
                hpack_decoder_.update_dynamic_table_size(value);
                break;
            case settings_frame::enable_push:
                enable_push_ = value != 0;
                break;
            case settings_frame::max_concurrent_streams:
                max_concurrent_streams_ = value;
                break;
            case settings_frame::initial_window_size:
                initial_window_size_ = value;
                break;
            case settings_frame::max_frame_size:
                max_frame_size_ = value;
                break;
            case settings_frame::max_header_list_size:
                max_header_list_size_ = value;
                break;
        }
    }
}

inline std::expected<std::vector<header_field>, error_code> hpack_decoder::decode(std::span<const uint8_t> data) {
    std::vector<header_field> headers;
    auto remaining = data;
    
    while (!remaining.empty()) {
        uint8_t first_byte = remaining[0];
        
        if (first_byte & 0x80) {
            // Indexed Header Field
            auto result = decode_indexed_header(remaining);
            if (!result) return std::unexpected(result.error());
            headers.push_back(*result);
        } else if (first_byte & 0x40) {
            // Literal Header Field with Incremental Indexing
            auto result = decode_literal_header(remaining, true);
            if (!result) return std::unexpected(result.error());
            headers.push_back(*result);
        } else {
            // Literal Header Field without Indexing
            auto result = decode_literal_header(remaining, false);
            if (!result) return std::unexpected(result.error());
            headers.push_back(*result);
        }
    }
    
    return headers;
}

inline stream_state& parser::get_or_create_stream(uint32_t stream_id) {
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        stream_state state;
        state.stream_id = stream_id;
        state.window_size = initial_window_size_;
        it = streams_.emplace(stream_id, std::move(state)).first;
    }
    return it->second;
}

} // namespace co::http::v2