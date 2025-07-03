#pragma once

#include "types.hpp"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>

namespace co::http::v2 {

// =============================================================================
// HTTP/2 Stream Manager (High Performance)
// =============================================================================

class stream_manager {
public:
    stream_manager() = default;
    ~stream_manager() = default;
    
    // Non-copyable, movable
    stream_manager(const stream_manager&) = delete;
    stream_manager& operator=(const stream_manager&) = delete;
    stream_manager(stream_manager&&) = default;
    stream_manager& operator=(stream_manager&&) = default;
    
    // =============================================================================
    // Stream Management
    // =============================================================================
    
    // Create a new stream
    std::expected<stream_info*, h2_error_code> create_stream(uint32_t stream_id, bool is_server = false) {
        // Validate stream ID
        if (stream_id == 0) {
            return std::unexpected(h2_error_code::protocol_error);
        }
        
        // Check for valid stream ID ordering
        if (is_server) {
            if (stream_id % 2 != 0) { // Server streams must be even
                return std::unexpected(h2_error_code::protocol_error);
            }
        } else {
            if (stream_id % 2 == 0) { // Client streams must be odd
                return std::unexpected(h2_error_code::protocol_error);
            }
        }
        
        // Check if stream already exists
        if (streams_.find(stream_id) != streams_.end()) {
            return std::unexpected(h2_error_code::protocol_error);
        }
        
        // Check stream ID ordering (must be greater than previous)
        if (stream_id <= last_stream_id_) {
            return std::unexpected(h2_error_code::protocol_error);
        }
        
        // Check concurrent streams limit
        if (active_streams_.size() >= settings_.max_concurrent_streams) {
            return std::unexpected(h2_error_code::refused_stream);
        }
        
        // Create new stream
        auto [it, inserted] = streams_.emplace(stream_id, stream_info{});
        if (!inserted) {
            return std::unexpected(h2_error_code::internal_error);
        }
        
        auto& stream = it->second;
        stream.id = stream_id;
        stream.state = stream_state::open;
        stream.window_size = settings_.initial_window_size;
        stream.remote_window_size = settings_.initial_window_size;
        
        active_streams_.insert(stream_id);
        last_stream_id_ = std::max(last_stream_id_, stream_id);
        
        return &stream;
    }
    
    // Get existing stream
    stream_info* get_stream(uint32_t stream_id) noexcept {
        auto it = streams_.find(stream_id);
        return (it != streams_.end()) ? &it->second : nullptr;
    }
    
    const stream_info* get_stream(uint32_t stream_id) const noexcept {
        auto it = streams_.find(stream_id);
        return (it != streams_.end()) ? &it->second : nullptr;
    }
    
    // Close stream
    void close_stream(uint32_t stream_id, h2_error_code error = h2_error_code::no_error) {
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            it->second.state = stream_state::closed;
            it->second.error = error;
            it->second.local_closed = true;
            it->second.remote_closed = true;
            
            active_streams_.erase(stream_id);
            
            // Add to cleanup queue for deferred cleanup
            cleanup_queue_.push(stream_id);
            
            // Perform cleanup if queue is getting large
            if (cleanup_queue_.size() > max_cleanup_queue_size_) {
                cleanup_closed_streams();
            }
        }
    }
    
    // Half-close stream
    void half_close_stream_local(uint32_t stream_id) {
        auto* stream = get_stream(stream_id);
        if (stream) {
            stream->local_closed = true;
            if (stream->state == stream_state::open) {
                stream->state = stream_state::half_closed_local;
            } else if (stream->state == stream_state::half_closed_remote) {
                stream->state = stream_state::closed;
                active_streams_.erase(stream_id);
            }
        }
    }
    
    void half_close_stream_remote(uint32_t stream_id) {
        auto* stream = get_stream(stream_id);
        if (stream) {
            stream->remote_closed = true;
            if (stream->state == stream_state::open) {
                stream->state = stream_state::half_closed_remote;
            } else if (stream->state == stream_state::half_closed_local) {
                stream->state = stream_state::closed;
                active_streams_.erase(stream_id);
            }
        }
    }
    
    // =============================================================================
    // Flow Control
    // =============================================================================
    
    // Update stream window size
    std::expected<void, h2_error_code> update_stream_window(uint32_t stream_id, int32_t delta) {
        auto* stream = get_stream(stream_id);
        if (!stream) {
            return std::unexpected(h2_error_code::protocol_error);
        }
        
        // Check for overflow
        int64_t new_window = static_cast<int64_t>(stream->window_size) + delta;
        if (new_window > protocol_limits::max_window_size || new_window < 0) {
            return std::unexpected(h2_error_code::flow_control_error);
        }
        
        stream->window_size = static_cast<int32_t>(new_window);
        return {};
    }
    
    // Update remote stream window size
    std::expected<void, h2_error_code> update_remote_stream_window(uint32_t stream_id, int32_t delta) {
        auto* stream = get_stream(stream_id);
        if (!stream) {
            return std::unexpected(h2_error_code::protocol_error);
        }
        
        int64_t new_window = static_cast<int64_t>(stream->remote_window_size) + delta;
        if (new_window > protocol_limits::max_window_size || new_window < 0) {
            return std::unexpected(h2_error_code::flow_control_error);
        }
        
        stream->remote_window_size = static_cast<int32_t>(new_window);
        return {};
    }
    
    // Consume stream window for outgoing data
    std::expected<uint32_t, h2_error_code> consume_stream_window(uint32_t stream_id, uint32_t size) {
        auto* stream = get_stream(stream_id);
        if (!stream) {
            return std::unexpected(h2_error_code::protocol_error);
        }
        
        if (!stream->can_send_data()) {
            return std::unexpected(h2_error_code::stream_closed);
        }
        
        // Calculate available window size
        uint32_t available = std::min(
            static_cast<uint32_t>(std::max(0, stream->window_size)),
            static_cast<uint32_t>(std::max(0, settings_.connection_window_size))
        );
        
        uint32_t to_consume = std::min(size, available);
        
        if (to_consume > 0) {
            stream->window_size -= to_consume;
            settings_.connection_window_size -= to_consume;
        }
        
        return to_consume;
    }
    
    // =============================================================================
    // Priority Management
    // =============================================================================
    
    void set_stream_priority(uint32_t stream_id, uint32_t dependency, uint8_t weight, bool exclusive) {
        auto* stream = get_stream(stream_id);
        if (stream) {
            stream->dependency = dependency;
            stream->weight = weight;
            stream->exclusive = exclusive;
            
            // TODO: Implement priority tree reordering for performance
            rebuild_priority_tree();
        }
    }
    
    std::vector<uint32_t> get_prioritized_streams() const {
        std::vector<uint32_t> result;
        result.reserve(active_streams_.size());
        
        // Simple priority implementation - can be optimized with proper priority tree
        for (uint32_t stream_id : active_streams_) {
            const auto* stream = get_stream(stream_id);
            if (stream && stream->can_send_data()) {
                result.push_back(stream_id);
            }
        }
        
        // Sort by weight (higher weight = higher priority)
        std::sort(result.begin(), result.end(), [this](uint32_t a, uint32_t b) {
            const auto* stream_a = get_stream(a);
            const auto* stream_b = get_stream(b);
            return stream_a->weight > stream_b->weight;
        });
        
        return result;
    }
    
    // =============================================================================
    // Settings Management
    // =============================================================================
    
    void update_settings(const connection_settings& new_settings) {
        // Update window sizes for existing streams when initial window size changes
        if (new_settings.initial_window_size != settings_.initial_window_size) {
            int32_t delta = static_cast<int32_t>(new_settings.initial_window_size) - 
                           static_cast<int32_t>(settings_.initial_window_size);
            
            for (auto& [stream_id, stream] : streams_) {
                int64_t new_window = static_cast<int64_t>(stream.window_size) + delta;
                // Clamp to valid range
                stream.window_size = static_cast<int32_t>(
                    std::max(static_cast<int64_t>(INT32_MIN),
                    std::min(static_cast<int64_t>(protocol_limits::max_window_size), new_window))
                );
            }
        }
        
        settings_ = new_settings;
    }
    
    const connection_settings& get_settings() const noexcept {
        return settings_;
    }
    
    // =============================================================================
    // Statistics and Monitoring
    // =============================================================================
    
    size_t active_stream_count() const noexcept {
        return active_streams_.size();
    }
    
    size_t total_stream_count() const noexcept {
        return streams_.size();
    }
    
    uint32_t get_last_stream_id() const noexcept {
        return last_stream_id_;
    }
    
    std::vector<uint32_t> get_active_stream_ids() const {
        return std::vector<uint32_t>(active_streams_.begin(), active_streams_.end());
    }
    
    // =============================================================================
    // Cleanup and Maintenance
    // =============================================================================
    
    void cleanup_closed_streams() {
        while (!cleanup_queue_.empty()) {
            uint32_t stream_id = cleanup_queue_.front();
            cleanup_queue_.pop();
            
            auto it = streams_.find(stream_id);
            if (it != streams_.end() && it->second.is_closed()) {
                streams_.erase(it);
            }
        }
    }
    
    void reset() {
        streams_.clear();
        active_streams_.clear();
        while (!cleanup_queue_.empty()) {
            cleanup_queue_.pop();
        }
        last_stream_id_ = 0;
        settings_ = connection_settings{};
    }
    
private:
    // Stream storage
    std::unordered_map<uint32_t, stream_info> streams_;
    std::unordered_set<uint32_t> active_streams_;
    std::queue<uint32_t> cleanup_queue_;
    
    // Connection state
    connection_settings settings_;
    uint32_t last_stream_id_ = 0;
    
    // Performance tuning
    static constexpr size_t max_cleanup_queue_size_ = 100;
    
    // Priority tree management
    void rebuild_priority_tree() {
        // TODO: Implement efficient priority tree for high-performance scheduling
        // This is a simplified implementation
    }
};

} // namespace co::http::v2