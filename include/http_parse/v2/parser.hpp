#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cstdint>
#include <array>
#include <functional>

#if __cplusplus >= 202002L
#include <concepts>
#include <expected>
#include <span>
#else
namespace std {
    template<typename T, typename E>
    class expected {
    public:
        using value_type = T;
        using error_type = E;
        
        expected(T value) : has_value_(true), value_(std::move(value)) {}
        expected(E error) : has_value_(false), error_(std::move(error)) {}
        
        bool has_value() const noexcept { return has_value_; }
        operator bool() const noexcept { return has_value_; }
        
        T& operator*() & { return value_; }
        const T& operator*() const & { return value_; }
        T&& operator*() && { return std::move(value_); }
        
        E& error() & { return error_; }
        const E& error() const & { return error_; }
        E&& error() && { return std::move(error_); }
        
    private:
        bool has_value_;
        union {
            T value_;
            E error_;
        };
    };
    
    
    template<typename T>
    class span {
    public:
        using element_type = T;
        using size_type = size_t;
        
        span() = default;
        span(T* data, size_type size) : data_(data), size_(size) {}
        
        T* data() const noexcept { return data_; }
        size_type size() const noexcept { return size_; }
        bool empty() const noexcept { return size_ == 0; }
        
        T& operator[](size_type idx) const { return data_[idx]; }
        
        span subspan(size_type offset) const {
            return span(data_ + offset, size_ - offset);
        }
        
    private:
        T* data_ = nullptr;
        size_type size_ = 0;
    };
}
#endif

namespace co::http::v2 {

enum class error_code {
    success = 0,
    need_more_data,
    protocol_error,
    internal_error,
    flow_control_error,
    settings_timeout,
    stream_closed,
    frame_size_error,
    refused_stream,
    cancel,
    compression_error,
    connect_error,
    enhance_your_calm,
    inadequate_security,
    http_1_1_required
};

enum class frame_type : uint8_t {
    data = 0x0,
    headers = 0x1,
    priority = 0x2,
    rst_stream = 0x3,
    settings = 0x4,
    push_promise = 0x5,
    ping = 0x6,
    goaway = 0x7,
    window_update = 0x8,
    continuation = 0x9
};

enum class frame_flags : uint8_t {
    none = 0x0,
    end_stream = 0x1,
    ack = 0x1,
    end_headers = 0x4,
    padded = 0x8,
    priority_flag = 0x20
};

struct frame_header {
    uint32_t length : 24;
    uint8_t type;
    uint8_t flags;
    uint32_t stream_id : 31;
    uint32_t reserved : 1;
    
    static constexpr size_t size = 9;
};

struct header_field {
    std::string name;
    std::string value;
    bool sensitive = false;
};

struct stream_state {
    uint32_t stream_id;
    bool closed = false;
    bool half_closed_local = false;
    bool half_closed_remote = false;
    int32_t window_size = 65535;
    std::vector<header_field> headers;
    std::string data;
};

struct settings_frame {
    static constexpr uint16_t header_table_size = 0x1;
    static constexpr uint16_t enable_push = 0x2;
    static constexpr uint16_t max_concurrent_streams = 0x3;
    static constexpr uint16_t initial_window_size = 0x4;
    static constexpr uint16_t max_frame_size = 0x5;
    static constexpr uint16_t max_header_list_size = 0x6;
    
    std::unordered_map<uint16_t, uint32_t> settings;
};

class hpack_decoder {
public:
    hpack_decoder() = default;
    
    std::expected<std::vector<header_field>, error_code> decode(std::span<const uint8_t> data);
    void update_dynamic_table_size(uint32_t size);
    
private:
    struct table_entry {
        std::string name;
        std::string value;
        size_t size() const { return name.size() + value.size() + 32; }
    };
    
    std::vector<table_entry> dynamic_table_;
    uint32_t dynamic_table_size_ = 4096;
    uint32_t max_dynamic_table_size_ = 4096;
    
    static const std::array<table_entry, 61> static_table_;
    
    std::expected<header_field, error_code> decode_literal_header(std::span<const uint8_t>& data, bool with_indexing);
    std::expected<header_field, error_code> decode_indexed_header(std::span<const uint8_t>& data);
    std::expected<std::string, error_code> decode_string(std::span<const uint8_t>& data);
    std::expected<uint32_t, error_code> decode_integer(std::span<const uint8_t>& data, uint8_t prefix_bits);
    
    const table_entry* get_table_entry(uint32_t index) const;
    void add_to_dynamic_table(const header_field& header);
    void evict_from_dynamic_table();
};

class parser {
public:
    parser() = default;
    
    std::expected<size_t, error_code> parse(std::span<const uint8_t> data);
    
    bool has_frame() const noexcept { return frame_complete_; }
    bool connection_preface_received() const noexcept { return preface_received_; }
    
    std::optional<stream_state> get_stream(uint32_t stream_id) const;
    std::vector<uint32_t> get_active_streams() const;
    
    void reset() noexcept;
    
    // Settings
    uint32_t get_setting(uint16_t id) const;
    void apply_settings(const settings_frame& settings);
    
    // Callbacks
    std::function<void(uint32_t stream_id, const std::vector<header_field>& headers)> on_headers;
    std::function<void(uint32_t stream_id, std::span<const uint8_t> data)> on_data;
    std::function<void(uint32_t stream_id, error_code error)> on_stream_error;
    std::function<void(error_code error)> on_connection_error;
    std::function<void(const settings_frame& settings)> on_settings;
    std::function<void(std::span<const uint8_t> data)> on_ping;
    std::function<void(uint32_t last_stream_id, error_code error)> on_goaway;
    
private:
    enum class state {
        preface,
        frame_header,
        frame_payload,
        complete
    };
    
    state state_ = state::preface;
    std::array<uint8_t, 24> preface_buffer_;
    size_t preface_bytes_ = 0;
    
    frame_header current_frame_;
    std::vector<uint8_t> frame_buffer_;
    size_t frame_bytes_ = 0;
    bool frame_complete_ = false;
    bool preface_received_ = false;
    
    std::unordered_map<uint32_t, stream_state> streams_;
    hpack_decoder hpack_decoder_;
    
    // Connection settings
    uint32_t header_table_size_ = 4096;
    bool enable_push_ = true;
    uint32_t max_concurrent_streams_ = UINT32_MAX;
    uint32_t initial_window_size_ = 65535;
    uint32_t max_frame_size_ = 16384;
    uint32_t max_header_list_size_ = UINT32_MAX;
    
    std::expected<size_t, error_code> parse_preface(std::span<const uint8_t> data);
    std::expected<size_t, error_code> parse_frame_header(std::span<const uint8_t> data);
    std::expected<size_t, error_code> parse_frame_payload(std::span<const uint8_t> data);
    
    std::expected<void, error_code> process_frame();
    std::expected<void, error_code> process_data_frame();
    std::expected<void, error_code> process_headers_frame();
    std::expected<void, error_code> process_settings_frame();
    std::expected<void, error_code> process_ping_frame();
    std::expected<void, error_code> process_goaway_frame();
    std::expected<void, error_code> process_window_update_frame();
    std::expected<void, error_code> process_rst_stream_frame();
    
    bool is_valid_stream_id(uint32_t stream_id) const;
    stream_state& get_or_create_stream(uint32_t stream_id);
};

} // namespace co::http::v2