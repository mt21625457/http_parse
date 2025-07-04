#pragma once

#include "frame_processor.hpp"
#include <cstring>
#include <algorithm>

namespace co::http::v2 {

// =============================================================================
// Frame Processor Implementation
// =============================================================================

inline frame_processor::frame_processor(std::unique_ptr<stream_manager> stream_mgr) 
    : stream_manager_(std::move(stream_mgr)) {
    if (!stream_manager_) {
        stream_manager_ = std::make_unique<stream_manager>();
    }
}

inline std::expected<size_t, h2_error_code> frame_processor::process_frames(std::span<const uint8_t> data) {
    size_t total_processed = 0;
    
    while (total_processed < data.size()) {
        auto remaining = data.subspan(total_processed);
        auto result = process_single_frame(remaining);
        
        if (!result) {
            return result;
        }
        
        if (*result == 0) {
            // Need more data
            break;
        }
        
        total_processed += *result;
        stats_.bytes_processed += *result;
    }
    
    return total_processed;
}

inline std::expected<size_t, h2_error_code> frame_processor::process_connection_preface(std::span<const uint8_t> data) {
    if (connection_preface_received_) {
        return 0;
    }
    
    size_t preface_size = connection_preface.size();
    size_t available = std::min(data.size(), preface_size - preface_bytes_received_);
    
    // Check if the preface matches
    if (std::memcmp(data.data(), connection_preface.data() + preface_bytes_received_, available) != 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    preface_bytes_received_ += available;
    
    if (preface_bytes_received_ == preface_size) {
        connection_preface_received_ = true;
    }
    
    return available;
}

inline std::expected<size_t, h2_error_code> frame_processor::process_single_frame(std::span<const uint8_t> data) {
    switch (state_) {
        case processing_state::expecting_header:
            return process_frame_header(data);
            
        case processing_state::expecting_payload:
            return process_frame_payload(data);
            
        case processing_state::expecting_continuation:
            // Process continuation frame
            if (data.size() < frame_header::size) {
                return 0; // Need more data
            }
            
            current_header_ = frame_header::parse(data.data());
            
            if (current_header_.type != static_cast<uint8_t>(frame_type::continuation)) {
                return std::unexpected{h2_error_code::protocol_error};
            }
            
            if (current_header_.stream_id != continuation_stream_id_) {
                return std::unexpected{h2_error_code::protocol_error};
            }
            
            state_ = processing_state::expecting_payload;
            bytes_needed_ = current_header_.length;
            return frame_header::size;
            
        default:
            return std::unexpected{h2_error_code::internal_error};
    }
}

inline std::expected<size_t, h2_error_code> frame_processor::process_frame_header(std::span<const uint8_t> data) {
    if (data.size() < frame_header::size) {
        return 0; // Need more data
    }
    
    current_header_ = frame_header::parse(data.data());
    
    // Validate frame header
    if (auto result = validate_frame_header(current_header_); !result) {
        return std::unexpected{result.error()};
    }
    
    state_ = processing_state::expecting_payload;
    bytes_needed_ = current_header_.length;
    frame_buffer_.clear();
    
    stats_.frames_processed++;
    
    return frame_header::size;
}

inline std::expected<size_t, h2_error_code> frame_processor::process_frame_payload(std::span<const uint8_t> data) {
    if (data.size() < bytes_needed_) {
        return 0; // Need more data
    }
    
    auto payload = data.subspan(0, bytes_needed_);
    
    // Process frame based on type
    std::expected<void, h2_error_code> result;
    
    switch (static_cast<frame_type>(current_header_.type)) {
        case frame_type::data:
            result = process_data_frame(current_header_, payload);
            stats_.data_frames++;
            break;
            
        case frame_type::headers:
            result = process_headers_frame(current_header_, payload);
            stats_.headers_frames++;
            break;
            
        case frame_type::priority:
            result = process_priority_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::rst_stream:
            result = process_rst_stream_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::settings:
            result = process_settings_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::push_promise:
            result = process_push_promise_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::ping:
            result = process_ping_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::goaway:
            result = process_goaway_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::window_update:
            result = process_window_update_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        case frame_type::continuation:
            result = process_continuation_frame(current_header_, payload);
            stats_.control_frames++;
            break;
            
        default:
            // Unknown frame type - ignore per RFC 7540
            result = {};
            break;
    }
    
    if (!result) {
        stats_.errors++;
        return std::unexpected{result.error()};
    }
    
    // Reset state for next frame
    if (!expecting_continuation_) {
        state_ = processing_state::expecting_header;
        bytes_needed_ = frame_header::size;
    }
    
    return bytes_needed_;
}

inline std::expected<void, h2_error_code> frame_processor::process_data_frame(const frame_header& header, std::span<const uint8_t> payload) {
    // Validate stream ID
    if (header.stream_id == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    // Check stream state
    if (auto result = validate_stream_state(header.stream_id, frame_type::data); !result) {
        return result;
    }
    
    // Handle padding
    size_t padding_length = 0;
    std::span<const uint8_t> data_payload = payload;
    
    if (header.flags & static_cast<uint8_t>(frame_flags::padded)) {
        if (payload.empty()) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        
        padding_length = payload[0];
        if (padding_length >= payload.size()) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        
        data_payload = payload.subspan(1, payload.size() - 1 - padding_length);
    }
    
    // Update stream window
    if (!data_payload.empty()) {
        auto window_result = stream_manager_->update_stream_window(header.stream_id, -static_cast<int32_t>(data_payload.size()));
        if (!window_result) {
            return std::unexpected{window_result.error()};
        }
    }
    
    // Handle end of stream
    bool end_stream = header.flags & static_cast<uint8_t>(frame_flags::end_stream);
    if (end_stream) {
        stream_manager_->half_close_stream_remote(header.stream_id);
    }
    
    // Call callback
    if (data_callback_) {
        data_callback_(header.stream_id, data_payload, end_stream);
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_headers_frame(const frame_header& header, std::span<const uint8_t> payload) {
    // Validate stream ID
    if (header.stream_id == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    // Check stream state
    if (auto result = validate_stream_state(header.stream_id, frame_type::headers); !result) {
        return result;
    }
    
    // Handle padding and priority
    size_t padding_length = 0;
    std::span<const uint8_t> headers_payload = payload;
    
    if (header.flags & static_cast<uint8_t>(frame_flags::padded)) {
        if (payload.empty()) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        
        padding_length = payload[0];
        if (padding_length >= payload.size()) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        
        headers_payload = payload.subspan(1);
    }
    
    if (header.flags & static_cast<uint8_t>(frame_flags::priority_flag)) {
        if (headers_payload.size() < 5) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        
        // Extract priority information
        uint32_t stream_dependency = (static_cast<uint32_t>(headers_payload[0]) << 24) |
                                    (static_cast<uint32_t>(headers_payload[1]) << 16) |
                                    (static_cast<uint32_t>(headers_payload[2]) << 8) |
                                    static_cast<uint32_t>(headers_payload[3]);
        
        bool exclusive = (stream_dependency & 0x80000000) != 0;
        stream_dependency &= 0x7FFFFFFF;
        uint8_t weight = headers_payload[4];
        
        // Set priority
        stream_manager_->set_stream_priority(header.stream_id, stream_dependency, weight, exclusive);
        
        headers_payload = headers_payload.subspan(5);
    }
    
    // Remove padding
    if (padding_length > 0) {
        if (headers_payload.size() < padding_length) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        headers_payload = headers_payload.subspan(0, headers_payload.size() - padding_length);
    }
    
    // Check if we need to wait for continuation frames
    bool end_headers = header.flags & static_cast<uint8_t>(frame_flags::end_headers);
    
    if (!end_headers) {
        // Start header block accumulation
        expecting_continuation_ = true;
        continuation_stream_id_ = header.stream_id;
        header_block_buffer_.clear();
        header_block_buffer_.insert(header_block_buffer_.end(), headers_payload.begin(), headers_payload.end());
        
        state_ = processing_state::expecting_continuation;
        return {};
    }
    
    // Complete header block
    std::span<const uint8_t> complete_header_block;
    if (header_block_buffer_.empty()) {
        complete_header_block = headers_payload;
    } else {
        header_block_buffer_.insert(header_block_buffer_.end(), headers_payload.begin(), headers_payload.end());
        complete_header_block = std::span<const uint8_t>(header_block_buffer_);
    }
    
    // Decode headers using HPACK
    auto decoded_headers = hpack_decoder_.decode_headers(complete_header_block);
    if (!decoded_headers) {
        return std::unexpected{h2_error_code::compression_error};
    }
    
    // Handle end of stream
    bool end_stream = header.flags & static_cast<uint8_t>(frame_flags::end_stream);
    if (end_stream) {
        stream_manager_->half_close_stream_remote(header.stream_id);
    }
    
    // Call callback
    if (headers_callback_) {
        headers_callback_(header.stream_id, *decoded_headers, end_stream, true);
    }
    
    // Clean up
    header_block_buffer_.clear();
    expecting_continuation_ = false;
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_settings_frame(const frame_header& header, std::span<const uint8_t> payload) {
    // Settings frames must have stream ID 0
    if (header.stream_id != 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    bool ack = header.flags & static_cast<uint8_t>(frame_flags::ack);
    
    if (ack) {
        // ACK frame must have empty payload
        if (!payload.empty()) {
            return std::unexpected{h2_error_code::frame_size_error};
        }
        
        // Call callback
        if (settings_callback_) {
            std::vector<setting> empty_settings;
            settings_callback_(empty_settings);
        }
        
        return {};
    }
    
    // Parse settings
    if (payload.size() % 6 != 0) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    std::unordered_map<uint16_t, uint32_t> settings;
    
    for (size_t i = 0; i < payload.size(); i += 6) {
        uint16_t id = (static_cast<uint16_t>(payload[i]) << 8) | static_cast<uint16_t>(payload[i + 1]);
        uint32_t value = (static_cast<uint32_t>(payload[i + 2]) << 24) |
                        (static_cast<uint32_t>(payload[i + 3]) << 16) |
                        (static_cast<uint32_t>(payload[i + 4]) << 8) |
                        static_cast<uint32_t>(payload[i + 5]);
        
        settings[id] = value;
    }
    
    // Validate and apply settings
    connection_settings new_settings = stream_manager_->get_settings();
    
    for (const auto& [id, value] : settings) {
        if (!new_settings.validate_setting(id, value)) {
            return std::unexpected{h2_error_code::protocol_error};
        }
        new_settings.apply_setting(id, value);
    }
    
    stream_manager_->update_settings(new_settings);
    
    // Call callback
    if (settings_callback_) {
        std::vector<setting> setting_list;
        for (const auto& [id, value] : settings) {
            setting_list.push_back({id, value});
        }
        settings_callback_(setting_list);
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::validate_frame_header(const frame_header& header) const {
    // Check frame size limits
    if (header.length > stream_manager_->get_settings().max_frame_size) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    // Check stream ID limits
    if (header.stream_id > protocol_limits::max_stream_id) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    // Frame type specific validations
    auto type = static_cast<frame_type>(header.type);
    
    switch (type) {
        case frame_type::data:
        case frame_type::headers:
        case frame_type::priority:
        case frame_type::rst_stream:
        case frame_type::push_promise:
        case frame_type::continuation:
            if (header.stream_id == 0) {
                return std::unexpected{h2_error_code::protocol_error};
            }
            break;
            
        case frame_type::settings:
        case frame_type::ping:
        case frame_type::goaway:
            if (header.stream_id != 0) {
                return std::unexpected{h2_error_code::protocol_error};
            }
            break;
            
        case frame_type::window_update:
            // Can be connection-level (stream_id = 0) or stream-level
            break;
            
        default:
            // Unknown frame types are allowed but ignored
            break;
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::validate_stream_state(uint32_t stream_id, frame_type type) const {
    auto* stream = stream_manager_->get_stream(stream_id);
    
    if (!stream) {
        // Stream doesn't exist - create it for certain frame types
        if (type == frame_type::headers) {
            return {}; // Will be created when processing
        }
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    // Check if stream can receive this frame type
    switch (type) {
        case frame_type::data:
            if (!stream->can_receive_data()) {
                return std::unexpected{h2_error_code::stream_closed};
            }
            break;
            
        case frame_type::headers:
            if (stream->is_closed()) {
                return std::unexpected{h2_error_code::stream_closed};
            }
            break;
            
        default:
            break;
    }
    
    return {};
}

inline void frame_processor::reset() {
    state_ = processing_state::expecting_header;
    bytes_needed_ = frame_header::size;
    frame_buffer_.clear();
    header_block_buffer_.clear();
    expecting_continuation_ = false;
    continuation_stream_id_ = 0;
    connection_preface_received_ = false;
    preface_bytes_received_ = 0;
    
    if (stream_manager_) {
        stream_manager_->reset();
    }
}

inline void frame_processor::update_settings(const connection_settings& settings) {
    if (stream_manager_) {
        stream_manager_->update_settings(settings);
    }
}

inline const connection_settings& frame_processor::get_settings() const noexcept {
    return stream_manager_->get_settings();
}

inline std::expected<void, h2_error_code> frame_processor::process_priority_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (header.stream_id == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    if (payload.size() != 5) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    uint32_t stream_dependency = (static_cast<uint32_t>(payload[0]) << 24) |
                                (static_cast<uint32_t>(payload[1]) << 16) |
                                (static_cast<uint32_t>(payload[2]) << 8) |
                                static_cast<uint32_t>(payload[3]);
    
    bool exclusive = (stream_dependency & 0x80000000) != 0;
    stream_dependency &= 0x7FFFFFFF;
    uint8_t weight = payload[4];
    
    stream_manager_->set_stream_priority(header.stream_id, stream_dependency, weight, exclusive);
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_rst_stream_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (header.stream_id == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    if (payload.size() != 4) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    uint32_t error_code = (static_cast<uint32_t>(payload[0]) << 24) |
                         (static_cast<uint32_t>(payload[1]) << 16) |
                         (static_cast<uint32_t>(payload[2]) << 8) |
                         static_cast<uint32_t>(payload[3]);
    
    stream_manager_->close_stream(header.stream_id, static_cast<h2_error_code>(error_code));
    
    if (error_callback_) {
        error_callback_(static_cast<h2_error_code>(error_code), "RST_STREAM frame received");
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_push_promise_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (header.stream_id == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    if (payload.size() < 4) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    uint32_t promised_stream_id = (static_cast<uint32_t>(payload[0]) << 24) |
                                 (static_cast<uint32_t>(payload[1]) << 16) |
                                 (static_cast<uint32_t>(payload[2]) << 8) |
                                 static_cast<uint32_t>(payload[3]);
    
    promised_stream_id &= 0x7FFFFFFF;
    
    auto headers_payload = payload.subspan(4);
    
    // Decode headers using HPACK
    auto decoded_headers = hpack_decoder_.decode_headers(headers_payload);
    if (!decoded_headers) {
        return std::unexpected{h2_error_code::compression_error};
    }
    
    if (push_promise_callback_) {
        push_promise_callback_(header.stream_id, promised_stream_id, *decoded_headers);
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_ping_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (header.stream_id != 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    if (payload.size() != 8) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    bool ack = header.flags & static_cast<uint8_t>(frame_flags::ack);
    
    if (ping_callback_) {
        std::array<uint8_t, 8> ping_data{};
        std::copy_n(payload.data(), std::min(payload.size(), size_t(8)), ping_data.data());
        ping_callback_(ping_data, ack);
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_goaway_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (header.stream_id != 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    if (payload.size() < 8) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    uint32_t last_stream_id = (static_cast<uint32_t>(payload[0]) << 24) |
                             (static_cast<uint32_t>(payload[1]) << 16) |
                             (static_cast<uint32_t>(payload[2]) << 8) |
                             static_cast<uint32_t>(payload[3]);
    
    last_stream_id &= 0x7FFFFFFF;
    
    uint32_t error_code = (static_cast<uint32_t>(payload[4]) << 24) |
                         (static_cast<uint32_t>(payload[5]) << 16) |
                         (static_cast<uint32_t>(payload[6]) << 8) |
                         static_cast<uint32_t>(payload[7]);
    
    auto debug_data = payload.subspan(8);
    
    if (goaway_callback_) {
        goaway_callback_(last_stream_id, static_cast<h2_error_code>(error_code), debug_data);
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_window_update_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (payload.size() != 4) {
        return std::unexpected{h2_error_code::frame_size_error};
    }
    
    uint32_t window_size_increment = (static_cast<uint32_t>(payload[0]) << 24) |
                                    (static_cast<uint32_t>(payload[1]) << 16) |
                                    (static_cast<uint32_t>(payload[2]) << 8) |
                                    static_cast<uint32_t>(payload[3]);
    
    window_size_increment &= 0x7FFFFFFF;
    
    if (window_size_increment == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    auto result = stream_manager_->update_stream_window(header.stream_id, window_size_increment);
    if (!result) {
        return std::unexpected{result.error()};
    }
    
    return {};
}

inline std::expected<void, h2_error_code> frame_processor::process_continuation_frame(const frame_header& header, std::span<const uint8_t> payload) {
    if (header.stream_id == 0) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    if (!expecting_continuation_ || header.stream_id != continuation_stream_id_) {
        return std::unexpected{h2_error_code::protocol_error};
    }
    
    header_block_buffer_.insert(header_block_buffer_.end(), payload.begin(), payload.end());
    
    bool end_headers = header.flags & static_cast<uint8_t>(frame_flags::end_headers);
    
    if (end_headers) {
        auto decoded_headers = hpack_decoder_.decode_headers(header_block_buffer_);
        if (!decoded_headers) {
            return std::unexpected{h2_error_code::compression_error};
        }
        
        if (headers_callback_) {
            headers_callback_(header.stream_id, *decoded_headers, false, false);
        }
        
        header_block_buffer_.clear();
        expecting_continuation_ = false;
        continuation_stream_id_ = 0;
        state_ = processing_state::expecting_header;
    }
    
    return {};
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_connection_preface(output_buffer& buffer) {
    buffer.append(connection_preface);
    return connection_preface.size();
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_headers_frame(uint32_t stream_id, const std::vector<header>& headers, bool end_stream, bool end_headers, output_buffer& buffer) {
    output_buffer temp_buffer;
    auto result = hpack_encoder_.encode_headers(headers, temp_buffer);
    if (!result) {
        return std::unexpected{h2_error_code::compression_error};
    }
    auto encoded_headers = temp_buffer.span();
    
    frame_header header;
    header.length = encoded_headers.size();
    header.type = static_cast<uint8_t>(frame_type::headers);
    header.flags = 0;
    if (end_stream) header.flags |= static_cast<uint8_t>(frame_flags::end_stream);
    if (end_headers) header.flags |= static_cast<uint8_t>(frame_flags::end_headers);
    header.stream_id = stream_id;
    
    buffer.append(header.serialize());
    buffer.append(encoded_headers);
    return frame_header::size + encoded_headers.size();
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_data_frame(uint32_t stream_id, std::span<const uint8_t> data, bool end_stream, output_buffer& buffer) {
    frame_header header;
    header.length = data.size();
    header.type = static_cast<uint8_t>(frame_type::data);
    header.flags = 0;
    if (end_stream) header.flags |= static_cast<uint8_t>(frame_flags::end_stream);
    header.stream_id = stream_id;
    
    buffer.append(header.serialize());
    buffer.append(data);
    return frame_header::size + data.size();
}

// Additional missing method implementations
inline std::expected<size_t, h2_error_code> frame_processor::generate_settings_frame(const std::unordered_map<uint16_t, uint32_t>& settings, bool ack, output_buffer& buffer) {
    frame_header header;
    header.length = ack ? 0 : settings.size() * 6;
    header.type = static_cast<uint8_t>(frame_type::settings);
    header.flags = ack ? static_cast<uint8_t>(frame_flags::ack) : 0;
    header.stream_id = 0;
    
    buffer.append(header.serialize());
    
    if (!ack) {
        for (const auto& [id, value] : settings) {
            uint8_t setting_data[6];
            setting_data[0] = static_cast<uint8_t>((id >> 8) & 0xFF);
            setting_data[1] = static_cast<uint8_t>(id & 0xFF);
            setting_data[2] = static_cast<uint8_t>((value >> 24) & 0xFF);
            setting_data[3] = static_cast<uint8_t>((value >> 16) & 0xFF);
            setting_data[4] = static_cast<uint8_t>((value >> 8) & 0xFF);
            setting_data[5] = static_cast<uint8_t>(value & 0xFF);
            buffer.append(std::span<const uint8_t>(setting_data, 6));
        }
    }
    
    return frame_header::size + header.length;
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_ping_frame(std::span<const uint8_t, 8> data, bool ack, output_buffer& buffer) {
    frame_header header;
    header.length = 8;
    header.type = static_cast<uint8_t>(frame_type::ping);
    header.flags = ack ? static_cast<uint8_t>(frame_flags::ack) : 0;
    header.stream_id = 0;
    
    buffer.append(header.serialize());
    buffer.append(std::span<const uint8_t>(data.data(), 8));
    
    return frame_header::size + 8;
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_goaway_frame(uint32_t last_stream_id, h2_error_code error_code, std::string_view debug_data, output_buffer& buffer) {
    frame_header header;
    header.length = 8 + debug_data.size();
    header.type = static_cast<uint8_t>(frame_type::goaway);
    header.flags = 0;
    header.stream_id = 0;
    
    buffer.append(header.serialize());
    
    // Last stream ID (31 bits)
    uint8_t goaway_data[8];
    goaway_data[0] = static_cast<uint8_t>((last_stream_id >> 24) & 0x7F);
    goaway_data[1] = static_cast<uint8_t>((last_stream_id >> 16) & 0xFF);
    goaway_data[2] = static_cast<uint8_t>((last_stream_id >> 8) & 0xFF);
    goaway_data[3] = static_cast<uint8_t>(last_stream_id & 0xFF);
    
    // Error code
    uint32_t error_val = static_cast<uint32_t>(error_code);
    goaway_data[4] = static_cast<uint8_t>((error_val >> 24) & 0xFF);
    goaway_data[5] = static_cast<uint8_t>((error_val >> 16) & 0xFF);
    goaway_data[6] = static_cast<uint8_t>((error_val >> 8) & 0xFF);
    goaway_data[7] = static_cast<uint8_t>(error_val & 0xFF);
    
    buffer.append(std::span<const uint8_t>(goaway_data, 8));
    buffer.append(debug_data);
    
    return frame_header::size + header.length;
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_window_update_frame(uint32_t stream_id, uint32_t window_size_increment, output_buffer& buffer) {
    frame_header header;
    header.length = 4;
    header.type = static_cast<uint8_t>(frame_type::window_update);
    header.flags = 0;
    header.stream_id = stream_id;
    
    buffer.append(header.serialize());
    
    uint8_t increment_data[4];
    increment_data[0] = static_cast<uint8_t>((window_size_increment >> 24) & 0x7F); // Clear reserved bit
    increment_data[1] = static_cast<uint8_t>((window_size_increment >> 16) & 0xFF);
    increment_data[2] = static_cast<uint8_t>((window_size_increment >> 8) & 0xFF);
    increment_data[3] = static_cast<uint8_t>(window_size_increment & 0xFF);
    
    buffer.append(std::span<const uint8_t>(increment_data, 4));
    
    return frame_header::size + 4;
}

inline std::expected<size_t, h2_error_code> frame_processor::generate_rst_stream_frame(uint32_t stream_id, h2_error_code error_code, output_buffer& buffer) {
    frame_header header;
    header.length = 4;
    header.type = static_cast<uint8_t>(frame_type::rst_stream);
    header.flags = 0;
    header.stream_id = stream_id;
    
    buffer.append(header.serialize());
    
    uint32_t error_val = static_cast<uint32_t>(error_code);
    uint8_t error_data[4];
    error_data[0] = static_cast<uint8_t>((error_val >> 24) & 0xFF);
    error_data[1] = static_cast<uint8_t>((error_val >> 16) & 0xFF);
    error_data[2] = static_cast<uint8_t>((error_val >> 8) & 0xFF);
    error_data[3] = static_cast<uint8_t>(error_val & 0xFF);
    
    buffer.append(std::span<const uint8_t>(error_data, 4));
    
    return frame_header::size + 4;
}

} // namespace co::http::v2