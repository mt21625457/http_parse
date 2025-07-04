#pragma once

#include "types.hpp"
#include "stream_manager.hpp"
#include "../detail/hpack.hpp"
#include "../buffer.hpp"
#include <functional>
#include <memory>

namespace co::http::v2 {

// =============================================================================
// HTTP/2 Frame Processor (High Performance)
// =============================================================================

class frame_processor {
public:
    // Callback types for frame processing
    using headers_callback = std::function<void(uint32_t stream_id, const std::vector<header>& headers, bool end_stream, bool end_headers)>;
    using data_callback = std::function<void(uint32_t stream_id, std::span<const uint8_t> data, bool end_stream)>;
    using priority_callback = std::function<void(uint32_t stream_id, uint32_t dependency, uint8_t weight, bool exclusive)>;
    using rst_stream_callback = std::function<void(uint32_t stream_id, h2_error_code error_code)>;
    using settings_callback = std::function<void(const std::unordered_map<uint16_t, uint32_t>& settings, bool ack)>;
    using push_promise_callback = std::function<void(uint32_t stream_id, uint32_t promised_stream_id, const std::vector<header>& headers)>;
    using ping_callback = std::function<void(std::span<const uint8_t, 8> data, bool ack)>;
    using goaway_callback = std::function<void(uint32_t last_stream_id, h2_error_code error_code, std::string_view debug_data)>;
    using window_update_callback = std::function<void(uint32_t stream_id, uint32_t window_size_increment)>;
    using continuation_callback = std::function<void(uint32_t stream_id, std::span<const uint8_t> header_block_fragment, bool end_headers)>;
    using connection_error_callback = std::function<void(h2_error_code error_code, std::string_view debug_info)>;

    frame_processor(std::unique_ptr<stream_manager> stream_mgr = std::make_unique<stream_manager>());
    ~frame_processor() = default;

    // Non-copyable, movable
    frame_processor(const frame_processor&) = delete;
    frame_processor& operator=(const frame_processor&) = delete;
    frame_processor(frame_processor&&) = default;
    frame_processor& operator=(frame_processor&&) = default;

    // =============================================================================
    // Callback Registration
    // =============================================================================

    void set_headers_callback(headers_callback callback) { headers_callback_ = std::move(callback); }
    void set_data_callback(data_callback callback) { data_callback_ = std::move(callback); }
    void set_priority_callback(priority_callback callback) { priority_callback_ = std::move(callback); }
    void set_rst_stream_callback(rst_stream_callback callback) { rst_stream_callback_ = std::move(callback); }
    void set_settings_callback(settings_callback callback) { settings_callback_ = std::move(callback); }
    void set_push_promise_callback(push_promise_callback callback) { push_promise_callback_ = std::move(callback); }
    void set_ping_callback(ping_callback callback) { ping_callback_ = std::move(callback); }
    void set_goaway_callback(goaway_callback callback) { goaway_callback_ = std::move(callback); }
    void set_window_update_callback(window_update_callback callback) { window_update_callback_ = std::move(callback); }
    void set_continuation_callback(continuation_callback callback) { continuation_callback_ = std::move(callback); }
    void set_connection_error_callback(connection_error_callback callback) { connection_error_callback_ = std::move(callback); }

    // =============================================================================
    // Frame Processing
    // =============================================================================

    // Process incoming frame data
    std::expected<size_t, h2_error_code> process_frames(std::span<const uint8_t> data);

    // Process connection preface
    std::expected<size_t, h2_error_code> process_connection_preface(std::span<const uint8_t> data);

    // =============================================================================
    // Frame Generation
    // =============================================================================

    // Generate DATA frame
    std::expected<size_t, h2_error_code> generate_data_frame(uint32_t stream_id, std::span<const uint8_t> data, 
                                                           bool end_stream, output_buffer& output);

    // Generate HEADERS frame  
    std::expected<size_t, h2_error_code> generate_headers_frame(uint32_t stream_id, const std::vector<header>& headers,
                                                              bool end_stream, bool end_headers, output_buffer& output);

    // Generate PRIORITY frame
    std::expected<size_t, h2_error_code> generate_priority_frame(uint32_t stream_id, uint32_t dependency,
                                                               uint8_t weight, bool exclusive, output_buffer& output);

    // Generate RST_STREAM frame
    std::expected<size_t, h2_error_code> generate_rst_stream_frame(uint32_t stream_id, h2_error_code error_code, 
                                                                 output_buffer& output);

    // Generate SETTINGS frame
    std::expected<size_t, h2_error_code> generate_settings_frame(const std::unordered_map<uint16_t, uint32_t>& settings,
                                                               bool ack, output_buffer& output);

    // Generate PUSH_PROMISE frame
    std::expected<size_t, h2_error_code> generate_push_promise_frame(uint32_t stream_id, uint32_t promised_stream_id,
                                                                   const std::vector<header>& headers, output_buffer& output);

    // Generate PING frame
    std::expected<size_t, h2_error_code> generate_ping_frame(std::span<const uint8_t, 8> data, bool ack, 
                                                           output_buffer& output);

    // Generate GOAWAY frame
    std::expected<size_t, h2_error_code> generate_goaway_frame(uint32_t last_stream_id, h2_error_code error_code,
                                                             std::string_view debug_data, output_buffer& output);

    // Generate WINDOW_UPDATE frame
    std::expected<size_t, h2_error_code> generate_window_update_frame(uint32_t stream_id, uint32_t window_size_increment,
                                                                    output_buffer& output);

    // Generate CONTINUATION frame
    std::expected<size_t, h2_error_code> generate_continuation_frame(uint32_t stream_id, std::span<const uint8_t> header_block_fragment,
                                                                   bool end_headers, output_buffer& output);

    // Generate connection preface
    std::expected<size_t, h2_error_code> generate_connection_preface(output_buffer& output);

    // =============================================================================
    // Configuration and State
    // =============================================================================

    // Update connection settings
    void update_settings(const connection_settings& settings);
    const connection_settings& get_settings() const noexcept;

    // Get stream manager
    stream_manager& get_stream_manager() noexcept { return *stream_manager_; }
    const stream_manager& get_stream_manager() const noexcept { return *stream_manager_; }

    // HPACK encoder/decoder access
    ::co::http::detail::hpack_encoder& get_hpack_encoder() noexcept { return hpack_encoder_; }
    ::co::http::detail::hpack_decoder& get_hpack_decoder() noexcept { return hpack_decoder_; }

    // Reset processor state
    void reset();

    // Get processing statistics
    struct stats {
        uint64_t frames_processed = 0;
        uint64_t bytes_processed = 0;
        uint64_t data_frames = 0;
        uint64_t headers_frames = 0;
        uint64_t control_frames = 0;
        uint64_t errors = 0;
    };
    
    const stats& get_stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = {}; }

private:
    // Frame processing state
    enum class processing_state {
        expecting_header,
        expecting_payload,
        expecting_continuation
    };

    processing_state state_ = processing_state::expecting_header;
    frame_header current_header_{};
    std::vector<uint8_t> frame_buffer_;
    size_t bytes_needed_ = frame_header::size;

    // Header continuation state
    uint32_t continuation_stream_id_ = 0;
    std::vector<uint8_t> header_block_buffer_;
    bool expecting_continuation_ = false;

    // Connection preface state
    bool connection_preface_received_ = false;
    size_t preface_bytes_received_ = 0;

    // Components
    std::unique_ptr<stream_manager> stream_manager_;
    ::co::http::detail::hpack_encoder hpack_encoder_;
    ::co::http::detail::hpack_decoder hpack_decoder_;

    // Callbacks
    headers_callback headers_callback_;
    data_callback data_callback_;
    priority_callback priority_callback_;
    rst_stream_callback rst_stream_callback_;
    settings_callback settings_callback_;
    push_promise_callback push_promise_callback_;
    ping_callback ping_callback_;
    goaway_callback goaway_callback_;
    window_update_callback window_update_callback_;
    continuation_callback continuation_callback_;
    connection_error_callback connection_error_callback_;

    // Statistics
    stats stats_;

    // =============================================================================
    // Frame Processing Implementation
    // =============================================================================

    std::expected<size_t, h2_error_code> process_single_frame(std::span<const uint8_t> data);
    std::expected<size_t, h2_error_code> process_frame_header(std::span<const uint8_t> data);
    std::expected<size_t, h2_error_code> process_frame_payload(std::span<const uint8_t> data);

    // Individual frame processors
    std::expected<void, h2_error_code> process_data_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_headers_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_priority_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_rst_stream_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_settings_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_push_promise_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_ping_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_goaway_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_window_update_frame(const frame_header& header, std::span<const uint8_t> payload);
    std::expected<void, h2_error_code> process_continuation_frame(const frame_header& header, std::span<const uint8_t> payload);

    // Frame validation
    std::expected<void, h2_error_code> validate_frame_header(const frame_header& header) const;
    std::expected<void, h2_error_code> validate_stream_state(uint32_t stream_id, frame_type type) const;

    // Utility methods
    void emit_connection_error(h2_error_code error_code, std::string_view debug_info = "");
    std::expected<void, h2_error_code> handle_headers_complete(uint32_t stream_id, bool end_stream);

    // Frame generation helpers
    void write_frame_header(const frame_header& header, output_buffer& output);
    size_t write_settings_payload(const std::unordered_map<uint16_t, uint32_t>& settings, output_buffer& output);
};

} // namespace co::http::v2

// Include implementation
#include "frame_processor_impl.hpp"