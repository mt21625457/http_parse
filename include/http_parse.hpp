#pragma once

// =============================================================================
// HTTP Parse Library - Elegant High-Level API
// A pure C++23 header-only HTTP/1.x and HTTP/2 protocol parsing library
// =============================================================================

#include "http_parse/core.hpp"
#include "http_parse/buffer.hpp"
#include "http_parse/parser.hpp"
#include "http_parse/encoder.hpp"
#include "http_parse/builder.hpp"
#include "http_parse/v2/frame_processor.hpp"
#include <memory>
#include <functional>
#include <concepts>

namespace co::http {

// =============================================================================
// HTTP/1.x Elegant Interface
// =============================================================================

namespace http1 {

    // =============================================================================
    // Simple Parse Functions (One-shot parsing)
    // =============================================================================
    
    inline std::expected<request, error_code> parse_request(std::string_view data) {
        parser p(version::http_1_1);
        return p.parse_request(data);
    }
    
    inline std::expected<response, error_code> parse_response(std::string_view data) {
        parser p(version::http_1_1);
        return p.parse_response(data);
    }
    
    // =============================================================================
    // Simple Encode Functions (One-shot encoding)
    // =============================================================================
    
    inline std::expected<std::string, error_code> encode_request(const request& req) {
        encoder enc(version::http_1_1);
        return enc.encode_request(req);
    }
    
    inline std::expected<std::string, error_code> encode_response(const response& resp) {
        encoder enc(version::http_1_1);
        return enc.encode_response(resp);
    }
    
    // High-performance buffer versions
    inline std::expected<size_t, error_code> encode_request(const request& req, output_buffer& buffer) {
        encoder enc(version::http_1_1);
        return enc.encode_request(req, buffer);
    }
    
    inline std::expected<size_t, error_code> encode_response(const response& resp, output_buffer& buffer) {
        encoder enc(version::http_1_1);
        return enc.encode_response(resp, buffer);
    }
    
    // =============================================================================
    // Streaming Parser (For incremental parsing)
    // =============================================================================
    
    template<typename MessageType>
    class stream_parser {
    public:
        explicit stream_parser() : parser_(version::http_1_1) {}
        
        // Parse incremental data
        std::expected<size_t, error_code> parse(std::span<const char> data, MessageType& msg) {
            if constexpr (std::is_same_v<MessageType, request>) {
                return parser_.parse_request_incremental(data, msg);
            } else {
                return parser_.parse_response_incremental(data, msg);
            }
        }
        
        // Parse incremental data (string_view convenience)
        std::expected<size_t, error_code> parse(std::string_view data, MessageType& msg) {
            return parse(std::span<const char>(data.data(), data.size()), msg);
        }
        
        // State queries
        bool is_complete() const noexcept { return parser_.is_parse_complete(); }
        bool needs_more_data() const noexcept { return parser_.needs_more_data(); }
        void reset() noexcept { parser_.reset(); }
        
    private:
        parser parser_;
    };
    
    using request_parser = stream_parser<request>;
    using response_parser = stream_parser<response>;
    
    // =============================================================================
    // Builder Pattern (Fluent API)
    // =============================================================================
    
    inline request_builder request() {
        return request_builder{};
    }
    
    inline response_builder response() {
        return response_builder{};
    }

} // namespace http1

// =============================================================================
// HTTP/2 Elegant Interface  
// =============================================================================

namespace http2 {

    // =============================================================================
    // Connection Management
    // =============================================================================
    
    class connection {
    public:
        // Connection lifecycle callbacks
        using on_stream_request_t = std::function<void(uint32_t stream_id, const request& req, bool end_stream)>;
        using on_stream_response_t = std::function<void(uint32_t stream_id, const response& resp, bool end_stream)>;
        using on_stream_data_t = std::function<void(uint32_t stream_id, std::span<const uint8_t> data, bool end_stream)>;
        using on_stream_end_t = std::function<void(uint32_t stream_id)>;
        using on_stream_error_t = std::function<void(uint32_t stream_id, v2::h2_error_code error)>;
        using on_connection_error_t = std::function<void(v2::h2_error_code error, std::string_view debug_info)>;
        using on_settings_t = std::function<void(const v2::connection_settings& settings)>;
        using on_ping_t = std::function<void(std::span<const uint8_t, 8> data, bool ack)>;
        using on_goaway_t = std::function<void(uint32_t last_stream_id, v2::h2_error_code error, std::string_view debug_data)>;
        
        connection(bool is_server = false) : is_server_(is_server), processor_(std::make_unique<v2::stream_manager>()) {
            setup_callbacks();
        }
        
        // =============================================================================
        // Event Handlers (Fluent API)
        // =============================================================================
        
        connection& on_request(on_stream_request_t handler) {
            on_stream_request_ = std::move(handler);
            return *this;
        }
        
        connection& on_response(on_stream_response_t handler) {
            on_stream_response_ = std::move(handler);
            return *this;
        }
        
        connection& on_data(on_stream_data_t handler) {
            on_stream_data_ = std::move(handler);
            return *this;
        }
        
        connection& on_stream_end(on_stream_end_t handler) {
            on_stream_end_ = std::move(handler);
            return *this;
        }
        
        connection& on_stream_error(on_stream_error_t handler) {
            on_stream_error_ = std::move(handler);
            return *this;
        }
        
        connection& on_connection_error(on_connection_error_t handler) {
            on_connection_error_ = std::move(handler);
            return *this;
        }
        
        connection& on_settings(on_settings_t handler) {
            on_settings_ = std::move(handler);
            return *this;
        }
        
        connection& on_ping(on_ping_t handler) {
            on_ping_ = std::move(handler);
            return *this;
        }
        
        connection& on_goaway(on_goaway_t handler) {
            on_goaway_ = std::move(handler);
            return *this;
        }
        
        // =============================================================================
        // Connection Operations
        // =============================================================================
        
        // Process incoming data
        std::expected<size_t, v2::h2_error_code> process(std::span<const uint8_t> data) {
            if (!preface_sent_ && !is_server_) {
                // Client must send preface first
                output_buffer buffer;
                if (auto result = processor_.generate_connection_preface(buffer); !result) {
                    return result;
                }
                preface_data_ = buffer.to_string();
                preface_sent_ = true;
            }
            
            if (!preface_received_) {
                auto result = processor_.process_connection_preface(data);
                if (!result) return result;
                
                if (*result == data.size()) {
                    preface_received_ = true;
                    return *result;
                }
                
                auto remaining = data.subspan(*result);
                auto frame_result = processor_.process_frames(remaining);
                if (!frame_result) return frame_result;
                
                return *result + *frame_result;
            }
            
            return processor_.process_frames(data);
        }
        
        // Get connection preface data (for clients)
        std::string_view get_preface() const noexcept {
            return preface_data_;
        }
        
        // =============================================================================
        // Stream Operations
        // =============================================================================
        
        // Send request (client-side)
        std::expected<output_buffer, v2::h2_error_code> send_request(const request& req, bool end_stream = true) {
            uint32_t stream_id = next_stream_id_;
            next_stream_id_ += 2; // Client streams are odd
            
            output_buffer buffer;
            
            // Convert request to headers
            std::vector<header> headers;
            headers.emplace_back(":method", method_string(req.method_));
            headers.emplace_back(":path", req.target_);
            headers.emplace_back(":scheme", "https"); // Default to HTTPS
            
            for (const auto& h : req.headers_) {
                headers.push_back(h);
            }
            
            auto result = processor_.generate_headers_frame(stream_id, headers, end_stream && req.body_.empty(), true, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            
            // Send body if present
            if (!req.body_.empty()) {
                auto body_span = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(req.body_.data()), req.body_.size());
                auto data_result = processor_.generate_data_frame(stream_id, body_span, end_stream, buffer);
                if (!data_result) {
                    return std::unexpected{data_result.error()};
                }
            }
            
            return buffer;
        }
        
        // Send response (server-side)
        std::expected<output_buffer, v2::h2_error_code> send_response(uint32_t stream_id, const response& resp, bool end_stream = true) {
            output_buffer buffer;
            
            // Convert response to headers
            std::vector<header> headers;
            headers.emplace_back(":status", std::to_string(static_cast<int>(resp.status_code_)));
            
            for (const auto& h : resp.headers_) {
                headers.push_back(h);
            }
            
            auto result = processor_.generate_headers_frame(stream_id, headers, end_stream && resp.body_.empty(), true, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            
            // Send body if present
            if (!resp.body_.empty()) {
                auto body_span = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(resp.body_.data()), resp.body_.size());
                auto data_result = processor_.generate_data_frame(stream_id, body_span, end_stream, buffer);
                if (!data_result) {
                    return std::unexpected{data_result.error()};
                }
            }
            
            return buffer;
        }
        
        // Send data frame
        std::expected<output_buffer, v2::h2_error_code> send_data(uint32_t stream_id, std::span<const uint8_t> data, bool end_stream = false) {
            output_buffer buffer;
            auto result = processor_.generate_data_frame(stream_id, data, end_stream, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        std::expected<output_buffer, v2::h2_error_code> send_data(uint32_t stream_id, std::string_view data, bool end_stream = false) {
            auto span = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());
            return send_data(stream_id, span, end_stream);
        }
        
        // =============================================================================
        // Control Operations
        // =============================================================================
        
        // Send settings
        std::expected<output_buffer, v2::h2_error_code> send_settings(const std::unordered_map<uint16_t, uint32_t>& settings) {
            output_buffer buffer;
            auto result = processor_.generate_settings_frame(settings, false, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        // Send settings ACK
        std::expected<output_buffer, v2::h2_error_code> send_settings_ack() {
            output_buffer buffer;
            auto result = processor_.generate_settings_frame({}, true, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        // Send ping
        std::expected<output_buffer, v2::h2_error_code> send_ping(std::span<const uint8_t, 8> data, bool ack = false) {
            output_buffer buffer;
            auto result = processor_.generate_ping_frame(data, ack, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        // Send GOAWAY
        std::expected<output_buffer, v2::h2_error_code> send_goaway(v2::h2_error_code error_code, std::string_view debug_data = "") {
            output_buffer buffer;
            auto result = processor_.generate_goaway_frame(last_processed_stream_id_, error_code, debug_data, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        // Send window update
        std::expected<output_buffer, v2::h2_error_code> send_window_update(uint32_t stream_id, uint32_t increment) {
            output_buffer buffer;
            auto result = processor_.generate_window_update_frame(stream_id, increment, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        // Send RST_STREAM
        std::expected<output_buffer, v2::h2_error_code> send_rst_stream(uint32_t stream_id, v2::h2_error_code error_code) {
            output_buffer buffer;
            auto result = processor_.generate_rst_stream_frame(stream_id, error_code, buffer);
            if (!result) {
                return std::unexpected{result.error()};
            }
            return buffer;
        }
        
        // =============================================================================
        // Configuration and Status
        // =============================================================================
        
        // Update settings
        void update_settings(const v2::connection_settings& settings) {
            processor_.update_settings(settings);
        }
        
        const v2::connection_settings& get_settings() const noexcept {
            return processor_.get_settings();
        }
        
        // Get stream manager
        const v2::stream_manager& get_stream_manager() const noexcept {
            return processor_.get_stream_manager();
        }
        
        // Get statistics
        const v2::frame_processor::stats& get_stats() const noexcept {
            return processor_.get_stats();
        }
        
        // Reset connection
        void reset() {
            processor_.reset();
            preface_sent_ = false;
            preface_received_ = false;
            next_stream_id_ = is_server_ ? 2 : 1;
            last_processed_stream_id_ = 0;
        }
        
    private:
        bool is_server_;
        bool preface_sent_ = false;
        bool preface_received_ = false;
        uint32_t next_stream_id_;
        uint32_t last_processed_stream_id_ = 0;
        std::string preface_data_;
        
        v2::frame_processor processor_;
        
        // Event handlers
        on_stream_request_t on_stream_request_;
        on_stream_response_t on_stream_response_;
        on_stream_data_t on_stream_data_;
        on_stream_end_t on_stream_end_;
        on_stream_error_t on_stream_error_;
        on_connection_error_t on_connection_error_;
        on_settings_t on_settings_;
        on_ping_t on_ping_;
        on_goaway_t on_goaway_;
        
        void setup_callbacks() {
            processor_.set_headers_callback([this](uint32_t stream_id, const std::vector<header>& headers, bool end_stream, bool end_headers) {
                last_processed_stream_id_ = std::max(last_processed_stream_id_, stream_id);
                
                // Parse headers into request/response
                if (is_server_) {
                    // Parse as request
                    request req;
                    for (const auto& h : headers) {
                        if (h.name == ":method") {
                            req.method_ = parse_method(h.value);
                        } else if (h.name == ":path") {
                            req.target_ = h.value;
                        } else if (h.name == ":scheme") {
                            // Store scheme if needed
                        } else if (h.name == ":authority") {
                            req.headers_.emplace_back("host", h.value);
                        } else if (!h.name.starts_with(':')) {
                            req.headers_.push_back(h);
                        }
                    }
                    
                    if (on_stream_request_) {
                        on_stream_request_(stream_id, req, end_stream);
                    }
                } else {
                    // Parse as response
                    response resp;
                    for (const auto& h : headers) {
                        if (h.name == ":status") {
                            resp.status_code_ = static_cast<status_code>(std::stoi(h.value));
                        } else if (!h.name.starts_with(':')) {
                            resp.headers_.push_back(h);
                        }
                    }
                    
                    if (on_stream_response_) {
                        on_stream_response_(stream_id, resp, end_stream);
                    }
                }
                
                if (end_stream && on_stream_end_) {
                    on_stream_end_(stream_id);
                }
            });
            
            processor_.set_data_callback([this](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
                if (on_stream_data_) {
                    on_stream_data_(stream_id, data, end_stream);
                }
                
                if (end_stream && on_stream_end_) {
                    on_stream_end_(stream_id);
                }
            });
            
            processor_.set_rst_stream_callback([this](uint32_t stream_id, v2::h2_error_code error_code) {
                if (on_stream_error_) {
                    on_stream_error_(stream_id, error_code);
                }
            });
            
            processor_.set_settings_callback([this](const std::unordered_map<uint16_t, uint32_t>& settings, bool ack) {
                if (!ack && on_settings_) {
                    v2::connection_settings conn_settings = processor_.get_settings();
                    for (const auto& [id, value] : settings) {
                        conn_settings.apply_setting(id, value);
                    }
                    on_settings_(conn_settings);
                }
            });
            
            processor_.set_ping_callback([this](std::span<const uint8_t, 8> data, bool ack) {
                if (on_ping_) {
                    on_ping_(data, ack);
                }
            });
            
            processor_.set_goaway_callback([this](uint32_t last_stream_id, v2::h2_error_code error_code, std::string_view debug_data) {
                if (on_goaway_) {
                    on_goaway_(last_stream_id, error_code, debug_data);
                }
            });
            
            processor_.set_connection_error_callback([this](v2::h2_error_code error_code, std::string_view debug_info) {
                if (on_connection_error_) {
                    on_connection_error_(error_code, debug_info);
                }
            });
        }
        
        method parse_method(const std::string& method_str) {
            if (method_str == "GET") return method::get;
            if (method_str == "POST") return method::post;
            if (method_str == "PUT") return method::put;
            if (method_str == "DELETE") return method::delete_;
            if (method_str == "HEAD") return method::head;
            if (method_str == "OPTIONS") return method::options;
            if (method_str == "TRACE") return method::trace;
            if (method_str == "CONNECT") return method::connect;
            if (method_str == "PATCH") return method::patch;
            return method::unknown;
        }
        
        std::string method_string(method m) {
            switch (m) {
                case method::get: return "GET";
                case method::post: return "POST";
                case method::put: return "PUT";
                case method::delete_: return "DELETE";
                case method::head: return "HEAD";
                case method::options: return "OPTIONS";
                case method::trace: return "TRACE";
                case method::connect: return "CONNECT";
                case method::patch: return "PATCH";
                case method::unknown: return "UNKNOWN";
            }
            return "UNKNOWN";
        }
    };
    
    // =============================================================================
    // Convenience Functions
    // =============================================================================
    
    // Create client connection
    inline connection client() {
        return connection(false);
    }
    
    // Create server connection
    inline connection server() {
        return connection(true);
    }

} // namespace http2

// =============================================================================
// Unified Convenience Interface
// =============================================================================

class http_parse {
public:
    // HTTP/1 shortcuts
    static auto parse_request(std::string_view data) { return http1::parse_request(data); }
    static auto parse_response(std::string_view data) { return http1::parse_response(data); }
    static auto encode_request(const request& req) { return http1::encode_request(req); }
    static auto encode_response(const response& resp) { return http1::encode_response(resp); }
    
    // Builder shortcuts
    static auto request() { return http1::request(); }
    static auto response() { return http1::response(); }
    
    // HTTP/2 shortcuts
    static auto http2_client() { return http2::client(); }
    static auto http2_server() { return http2::server(); }
    
    // Utility functions
    static std::string version_string(version v) {
        switch (v) {
            case version::http_1_0: return "HTTP/1.0";
            case version::http_1_1: return "HTTP/1.1"; 
            case version::http_2_0: return "HTTP/2.0";
            case version::auto_detect: return "AUTO";
        }
        return "UNKNOWN";
    }
    
    static std::string error_string(error_code e) {
        switch (e) {
            case error_code::success: return "Success";
            case error_code::need_more_data: return "Need more data";
            case error_code::protocol_error: return "Protocol error";
            case error_code::invalid_method: return "Invalid method";
            case error_code::invalid_uri: return "Invalid URI";
            case error_code::invalid_version: return "Invalid version";
            case error_code::invalid_header: return "Invalid header";
            case error_code::invalid_body: return "Invalid body";
            case error_code::frame_size_error: return "Frame size error";
            case error_code::compression_error: return "Compression error";
            case error_code::flow_control_error: return "Flow control error";
            case error_code::stream_closed: return "Stream closed";
            case error_code::connection_error: return "Connection error";
        }
        return "Unknown error";
    }
};

} // namespace co::http