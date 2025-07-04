#pragma once

// =============================================================================
// HTTP Parse Library - Elegant High-Level API
// HTTP解析库 - 优雅的高级API
// 
// A pure C++23 header-only HTTP/1.x and HTTP/2 protocol parsing library
// 一个纯C++23头文件HTTP/1.x和HTTP/2协议解析库
//
// Features / 特性:
// - RFC7540/RFC7541/RFC9113 compliant HTTP/2 implementation
//   符合RFC7540/RFC7541/RFC9113标准的HTTP/2实现
// - High-performance zero-copy parsing and encoding
//   高性能零拷贝解析和编码
// - HPACK header compression support
//   HPACK头部压缩支持
// - Stream multiplexing and flow control
//   流多路复用和流量控制
// - Modern C++23 with std::expected error handling
//   现代C++23设计，使用std::expected错误处理
// - Thread-safe concurrent operations
//   线程安全的并发操作
// =============================================================================

#include "http_parse/buffer.hpp"
#include "http_parse/builder.hpp"
#include "http_parse/core.hpp"
#include "http_parse/encoder.hpp"
#include "http_parse/parser.hpp"
#include "http_parse/v2/frame_processor.hpp"
#include <concepts>
#include <functional>
#include <memory>

namespace co::http {

// =============================================================================
// HTTP/1.x Elegant Interface / HTTP/1.x优雅接口
// Provides simple and efficient HTTP/1.x parsing and encoding functionality
// 提供简单高效的HTTP/1.x解析和编码功能
// =============================================================================

namespace http1 {

// =============================================================================
// Simple Parse Functions (One-shot parsing) / 简单解析函数（一次性解析）
// High-level functions for parsing complete HTTP messages in memory
// 用于解析内存中完整HTTP消息的高级函数
// =============================================================================

/**
 * @brief Parse a complete HTTP/1.x request from string data
 * @brief 从字符串数据解析完整的HTTP/1.x请求
 * 
 * @param data The raw HTTP request data / 原始HTTP请求数据
 * @return std::expected<request, error_code> Parsed request or error / 解析的请求或错误
 * 
 * @details This function performs complete parsing of an HTTP/1.x request including:
 * @details 此函数执行HTTP/1.x请求的完整解析，包括：
 * - Request line (method, URI, version) / 请求行（方法、URI、版本）
 * - Headers parsing with case-insensitive names / 头部解析（名称大小写不敏感）
 * - Body extraction based on Content-Length / 基于Content-Length的正文提取
 * - Automatic handling of common headers / 常见头部的自动处理
 * 
 * @example
 * auto result = http1::parse_request("GET /api/users HTTP/1.1\r\nHost: example.com\r\n\r\n");
 * if (result) {
 *   const auto& req = result.value();
 *   std::cout << "Method: " << static_cast<int>(req.method_) << std::endl;
 * }
 */
inline std::expected<request, error_code> parse_request(std::string_view data) {
  parser p(version::http_1_1);
  return p.parse_request(data);
}

/**
 * @brief Parse a complete HTTP/1.x response from string data
 * @brief 从字符串数据解析完整的HTTP/1.x响应
 * 
 * @param data The raw HTTP response data / 原始HTTP响应数据
 * @return std::expected<response, error_code> Parsed response or error / 解析的响应或错误
 * 
 * @details This function performs complete parsing of an HTTP/1.x response including:
 * @details 此函数执行HTTP/1.x响应的完整解析，包括：
 * - Status line (version, status code, reason phrase) / 状态行（版本、状态码、原因短语）
 * - Headers parsing with automatic lowercase conversion / 头部解析（自动转换为小写）
 * - Body extraction with various encoding support / 支持多种编码的正文提取
 * - Transfer-Encoding: chunked support / 支持Transfer-Encoding: chunked
 * 
 * @example
 * auto result = http1::parse_response("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!");
 * if (result) {
 *   std::cout << "Status: " << static_cast<int>(result.value().status_code_) << std::endl;
 * }
 */
inline std::expected<response, error_code>
parse_response(std::string_view data) {
  parser p(version::http_1_1);
  return p.parse_response(data);
}

// =============================================================================
// Simple Encode Functions (One-shot encoding) / 简单编码函数（一次性编码）
// High-level functions for encoding HTTP messages to wire format
// 用于将HTTP消息编码为线路格式的高级函数
// =============================================================================

/**
 * @brief Encode an HTTP/1.x request to string format
 * @brief 将HTTP/1.x请求编码为字符串格式
 * 
 * @param req The request object to encode / 要编码的请求对象
 * @return std::expected<std::string, error_code> Encoded request string or error / 编码的请求字符串或错误
 * 
 * @details This function serializes a request object into valid HTTP/1.x format:
 * @details 此函数将请求对象序列化为有效的HTTP/1.x格式：
 * - Generates proper request line / 生成正确的请求行
 * - Formats all headers with CRLF line endings / 使用CRLF行结束符格式化所有头部
 * - Automatically adds Content-Length if body is present / 如果存在正文则自动添加Content-Length
 * - Ensures HTTP/1.1 compliance / 确保HTTP/1.1合规性
 * 
 * @example
 * request req;
 * req.method_ = method::post;
 * req.target_ = "/api/users";
 * req.headers_ = {{"host", "api.example.com"}};
 * req.body_ = R"({"name": "user"})";
 * auto result = http1::encode_request(req);
 */
inline std::expected<std::string, error_code>
encode_request(const request &req) {
  encoder enc(version::http_1_1);
  return enc.encode_request(req);
}

/**
 * @brief Encode an HTTP/1.x response to string format
 * @brief 将HTTP/1.x响应编码为字符串格式
 * 
 * @param resp The response object to encode / 要编码的响应对象
 * @return std::expected<std::string, error_code> Encoded response string or error / 编码的响应字符串或错误
 * 
 * @details This function serializes a response object into valid HTTP/1.x format:
 * @details 此函数将响应对象序列化为有效的HTTP/1.x格式：
 * - Generates proper status line with reason phrase / 生成带原因短语的正确状态行
 * - Formats headers in canonical form / 以标准形式格式化头部
 * - Handles various content encodings / 处理各种内容编码
 * - Optimizes for common response patterns / 针对常见响应模式进行优化
 * 
 * @example
 * response resp;
 * resp.status_code_ = status_code::ok;
 * resp.headers_ = {{"content-type", "application/json"}};
 * resp.body_ = R"({"status": "success"})";
 * auto result = http1::encode_response(resp);
 */
inline std::expected<std::string, error_code>
encode_response(const response &resp) {
  encoder enc(version::http_1_1);
  return enc.encode_response(resp);
}

// =============================================================================
// High-performance buffer versions / 高性能缓冲区版本
// Zero-copy encoding directly to output buffers for maximum efficiency
// 零拷贝编码直接到输出缓冲区以获得最大效率
// =============================================================================

/**
 * @brief Encode an HTTP/1.x request directly to output buffer (zero-copy)
 * @brief 将HTTP/1.x请求直接编码到输出缓冲区（零拷贝）
 * 
 * @param req The request object to encode / 要编码的请求对象
 * @param buffer The output buffer to write to / 要写入的输出缓冲区
 * @return std::expected<size_t, error_code> Number of bytes written or error / 写入的字节数或错误
 * 
 * @details High-performance version that avoids string allocation:
 * @details 避免字符串分配的高性能版本：
 * - Direct buffer writing without intermediate allocations / 直接缓冲区写入，无中间分配
 * - Optimal for high-throughput servers / 最适合高吞吐量服务器
 * - Reusable buffer for multiple operations / 可重用缓冲区进行多次操作
 * - Memory-efficient streaming / 内存高效的流式处理
 */
inline std::expected<size_t, error_code> encode_request(const request &req,
                                                        output_buffer &buffer) {
  encoder enc(version::http_1_1);
  return enc.encode_request(req, buffer);
}

/**
 * @brief Encode an HTTP/1.x response directly to output buffer (zero-copy)
 * @brief 将HTTP/1.x响应直接编码到输出缓冲区（零拷贝）
 * 
 * @param resp The response object to encode / 要编码的响应对象
 * @param buffer The output buffer to write to / 要写入的输出缓冲区
 * @return std::expected<size_t, error_code> Number of bytes written or error / 写入的字节数或错误
 * 
 * @details High-performance version optimized for server applications:
 * @details 针对服务器应用程序优化的高性能版本：
 * - Minimal memory footprint / 最小内存占用
 * - Suitable for connection pooling / 适用于连接池
 * - Buffer can be reused across requests / 缓冲区可在请求间重用
 * - Supports chunked transfer encoding / 支持分块传输编码
 */
inline std::expected<size_t, error_code>
encode_response(const response &resp, output_buffer &buffer) {
  encoder enc(version::http_1_1);
  return enc.encode_response(resp, buffer);
}

// =============================================================================
// Streaming Parser (For incremental parsing) / 流式解析器（用于增量解析）
// Template-based parser for handling streaming HTTP data efficiently
// 基于模板的解析器，用于高效处理流式HTTP数据
// =============================================================================

/**
 * @brief Template stream parser for incremental HTTP message parsing
 * @brief 用于增量HTTP消息解析的模板流解析器
 * 
 * @tparam MessageType Either request or response type / 请求或响应类型
 * 
 * @details This parser is designed for handling streaming HTTP data where:
 * @details 此解析器专为处理流式HTTP数据而设计，适用于：
 * - Data arrives in chunks over network / 数据通过网络分块到达
 * - Memory-efficient processing of large messages / 大消息的内存高效处理
 * - Real-time parsing without buffering entire message / 实时解析无需缓冲整个消息
 * - Connection keep-alive scenarios / 连接保持活跃的场景
 * 
 * @example
 * http1::request_parser parser;
 * request req;
 * while (auto chunk = read_from_socket()) {
 *   auto result = parser.parse(chunk, req);
 *   if (result && parser.is_complete()) {
 *     // Request fully parsed
 *     break;
 *   }
 * }
 */
template <typename MessageType> class stream_parser {
public:
  /**
   * @brief Construct a new stream parser for HTTP/1.1
   * @brief 构造新的HTTP/1.1流解析器
   */
  explicit stream_parser() : parser_(version::http_1_1) {}

  /**
   * @brief Parse incremental data chunk
   * @brief 解析增量数据块
   * 
   * @param data Input data chunk / 输入数据块
   * @param msg Output message object to populate / 要填充的输出消息对象
   * @return std::expected<size_t, error_code> Bytes consumed or error / 消耗的字节数或错误
   * 
   * @details Processes data incrementally, maintaining internal state:
   * @details 增量处理数据，维护内部状态：
   * - Automatically handles partial headers / 自动处理部分头部
   * - Manages body parsing based on Content-Length / 基于Content-Length管理正文解析
   * - Supports chunked transfer encoding / 支持分块传输编码
   * - Thread-safe for single parser instance / 单个解析器实例线程安全
   */
  std::expected<size_t, error_code> parse(std::span<const char> data,
                                          MessageType &msg) {
    if constexpr (std::is_same_v<MessageType, request>) {
      return parser_.parse_request_incremental(data, msg);
    } else {
      return parser_.parse_response_incremental(data, msg);
    }
  }

  /**
   * @brief Parse incremental data chunk (string_view convenience overload)
   * @brief 解析增量数据块（string_view便利重载）
   * 
   * @param data Input data as string_view / 作为string_view的输入数据
   * @param msg Output message object to populate / 要填充的输出消息对象
   * @return std::expected<size_t, error_code> Bytes consumed or error / 消耗的字节数或错误
   */
  std::expected<size_t, error_code> parse(std::string_view data,
                                          MessageType &msg) {
    return parse(std::span<const char>(data.data(), data.size()), msg);
  }

  /**
   * @brief Check if message parsing is complete
   * @brief 检查消息解析是否完成
   * 
   * @return true if complete message has been parsed / 如果已解析完整消息则返回true
   * @return false if more data is needed / 如果需要更多数据则返回false
   */
  bool is_complete() const noexcept { return parser_.is_parse_complete(); }
  
  /**
   * @brief Check if parser needs more data to continue
   * @brief 检查解析器是否需要更多数据才能继续
   * 
   * @return true if more data is required / 如果需要更多数据则返回true
   * @return false if parser is in error state or complete / 如果解析器处于错误状态或完成则返回false
   */
  bool needs_more_data() const noexcept { return parser_.needs_more_data(); }
  
  /**
   * @brief Reset parser state for reuse
   * @brief 重置解析器状态以便重用
   * 
   * @details Clears all internal state, allowing parser to be reused:
   * @details 清除所有内部状态，允许解析器重用：
   * - Resets state machine to initial state / 将状态机重置为初始状态
   * - Clears any buffered data / 清除任何缓冲数据
   * - Suitable for connection keep-alive / 适用于连接保持活跃
   */
  void reset() noexcept { parser_.reset(); }

private:
  parser parser_;
};

/**
 * @brief Type alias for HTTP request stream parser
 * @brief HTTP请求流解析器的类型别名
 */
using request_parser = stream_parser<request>;

/**
 * @brief Type alias for HTTP response stream parser  
 * @brief HTTP响应流解析器的类型别名
 */
using response_parser = stream_parser<response>;

// =============================================================================
// Builder Pattern (Fluent API) / 构建器模式（流畅API）
// Convenient builder functions for constructing HTTP messages
// 用于构造HTTP消息的便利构建器函数
// =============================================================================

/**
 * @brief Create a new HTTP request builder
 * @brief 创建新的HTTP请求构建器
 * 
 * @return request_builder A fluent API builder for HTTP requests / HTTP请求的流畅API构建器
 * 
 * @details The request builder provides a fluent interface for constructing HTTP requests:
 * @details 请求构建器为构造HTTP请求提供流畅接口：
 * - Method chaining for easy configuration / 方法链接便于配置
 * - Automatic header management / 自动头部管理
 * - Built-in validation / 内置验证
 * - Support for various content types / 支持各种内容类型
 * 
 * @example
 * auto req = http1::request()
 *   .method(method::post)
 *   .target("/api/users")
 *   .header("host", "api.example.com")
 *   .json_body(R"({"name": "user"})")
 *   .build();
 */
inline request_builder request() { return request_builder{}; }

/**
 * @brief Create a new HTTP response builder
 * @brief 创建新的HTTP响应构建器
 * 
 * @return response_builder A fluent API builder for HTTP responses / HTTP响应的流畅API构建器
 * 
 * @details The response builder provides a fluent interface for constructing HTTP responses:
 * @details 响应构建器为构造HTTP响应提供流畅接口：
 * - Convenient status code setting / 便利的状态码设置
 * - Automatic content-type detection / 自动内容类型检测
 * - Cookie management / Cookie管理
 * - Security header helpers / 安全头部助手
 * 
 * @example
 * auto resp = http1::response()
 *   .status(status_code::ok)
 *   .header("content-type", "application/json")
 *   .body(R"({"result": "success"})")
 *   .build();
 */
inline response_builder response() { return response_builder{}; }

} // namespace http1

// =============================================================================
// HTTP/2 Elegant Interface / HTTP/2优雅接口
// Advanced HTTP/2 implementation with full RFC7540/RFC7541 support
// 具有完整RFC7540/RFC7541支持的高级HTTP/2实现
// =============================================================================

namespace http2 {

// =============================================================================
// Connection Management / 连接管理
// Full-featured HTTP/2 connection with stream multiplexing and flow control
// 具有流多路复用和流量控制的全功能HTTP/2连接
// =============================================================================

/**
 * @brief HTTP/2 connection handler with complete protocol support
 * @brief 具有完整协议支持的HTTP/2连接处理器
 * 
 * @details This class provides a complete HTTP/2 implementation including:
 * @details 此类提供完整的HTTP/2实现，包括：
 * - HPACK header compression (RFC7541) / HPACK头部压缩（RFC7541）
 * - Stream multiplexing and prioritization / 流多路复用和优先级
 * - Flow control per stream and connection / 每个流和连接的流量控制
 * - Server push support / 服务器推送支持
 * - Settings negotiation / 设置协商
 * - Error recovery and connection management / 错误恢复和连接管理
 * 
 * @note This class is not thread-safe. Use separate instances per thread.
 * @note 此类不是线程安全的。每个线程使用单独的实例。
 * 
 * @example
 * // Client usage
 * auto client = http2::client()
 *   .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
 *     std::cout << "Received response for stream " << stream_id << std::endl;
 *   })
 *   .on_error([](uint32_t stream_id, h2_error_code error) {
 *     std::cerr << "Stream error: " << static_cast<int>(error) << std::endl;
 *   });
 * 
 * auto buffer = client.send_request(my_request);
 * // Send buffer contents to server...
 */
class connection {
public:
  // =============================================================================
  // Callback Type Definitions / 回调类型定义
  // Event handlers for various HTTP/2 protocol events
  // 各种HTTP/2协议事件的事件处理器
  // =============================================================================
  
  /**
   * @brief Callback for incoming stream requests (server-side)
   * @brief 传入流请求的回调（服务器端）
   * @param stream_id Unique stream identifier / 唯一的流标识符
   * @param req Parsed HTTP request / 解析的HTTP请求
   * @param end_stream Whether this is the final frame for the stream / 这是否是流的最终帧
   */
  using on_stream_request_t = std::function<void(
      uint32_t stream_id, const request &req, bool end_stream)>;
  
  /**
   * @brief Callback for incoming stream responses (client-side)
   * @brief 传入流响应的回调（客户端）
   * @param stream_id Unique stream identifier / 唯一的流标识符
   * @param resp Parsed HTTP response / 解析的HTTP响应
   * @param end_stream Whether this is the final frame for the stream / 这是否是流的最终帧
   */
  using on_stream_response_t = std::function<void(
      uint32_t stream_id, const response &resp, bool end_stream)>;
  
  /**
   * @brief Callback for incoming stream data
   * @brief 传入流数据的回调
   * @param stream_id Unique stream identifier / 唯一的流标识符
   * @param data Raw data bytes / 原始数据字节
   * @param end_stream Whether this is the final data for the stream / 这是否是流的最终数据
   */
  using on_stream_data_t = std::function<void(
      uint32_t stream_id, std::span<const uint8_t> data, bool end_stream)>;
  
  /**
   * @brief Callback for stream end events
   * @brief 流结束事件的回调
   * @param stream_id Stream that has ended / 已结束的流
   */
  using on_stream_end_t = std::function<void(uint32_t stream_id)>;
  
  /**
   * @brief Callback for stream-level errors
   * @brief 流级别错误的回调
   * @param stream_id Stream with error / 出错的流
   * @param error Specific HTTP/2 error code / 特定的HTTP/2错误码
   */
  using on_stream_error_t =
      std::function<void(uint32_t stream_id, v2::h2_error_code error)>;
  
  /**
   * @brief Callback for connection-level errors
   * @brief 连接级别错误的回调
   * @param error HTTP/2 error code / HTTP/2错误码
   * @param debug_info Additional debug information / 附加调试信息
   */
  using on_connection_error_t =
      std::function<void(v2::h2_error_code error, std::string_view debug_info)>;
  
  /**
   * @brief Callback for settings updates
   * @brief 设置更新的回调
   * @param settings New connection settings / 新的连接设置
   */
  using on_settings_t =
      std::function<void(const v2::connection_settings &settings)>;
  
  /**
   * @brief Callback for ping frames
   * @brief ping帧的回调
   * @param data 8-byte ping payload / 8字节ping载荷
   * @param ack Whether this is a ping acknowledgment / 这是否是ping确认
   */
  using on_ping_t =
      std::function<void(std::span<const uint8_t, 8> data, bool ack)>;
  
  /**
   * @brief Callback for GOAWAY frames (connection shutdown)
   * @brief GOAWAY帧的回调（连接关闭）
   * @param last_stream_id Last processed stream ID / 最后处理的流ID
   * @param error Reason for connection closure / 连接关闭的原因
   * @param debug_data Optional debug information / 可选的调试信息
   */
  using on_goaway_t =
      std::function<void(uint32_t last_stream_id, v2::h2_error_code error,
                         std::string_view debug_data)>;

  /**
   * @brief Construct a new HTTP/2 connection
   * @brief 构造新的HTTP/2连接
   * 
   * @param is_server Whether this is a server-side connection / 这是否是服务器端连接
   * 
   * @details Initializes the connection with default settings:
   * @details 使用默认设置初始化连接：
   * - Sets up frame processor and stream manager / 设置帧处理器和流管理器
   * - Configures callback routing / 配置回调路由
   * - Prepares for HTTP/2 communication / 准备HTTP/2通信
   */
  connection(bool is_server = false)
      : is_server_(is_server),
        processor_(std::make_unique<v2::stream_manager>()) {
    setup_callbacks();
  }

  // =============================================================================
  // Event Handlers (Fluent API) / 事件处理器（流畅API）
  // Chainable methods for setting up event callbacks
  // 用于设置事件回调的可链式方法
  // =============================================================================

  /**
   * @brief Set callback for incoming HTTP requests (server-side)
   * @brief 设置传入HTTP请求的回调（服务器端）
   * 
   * @param handler Callback function to handle requests / 处理请求的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called when a complete request headers frame is received
   * @details 接收到完整的请求头部帧时调用
   */
  connection &on_request(on_stream_request_t handler) {
    on_stream_request_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for incoming HTTP responses (client-side)
   * @brief 设置传入HTTP响应的回调（客户端）
   * 
   * @param handler Callback function to handle responses / 处理响应的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called when response headers are received from server
   * @details 从服务器接收到响应头部时调用
   */
  connection &on_response(on_stream_response_t handler) {
    on_stream_response_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for incoming stream data
   * @brief 设置传入流数据的回调
   * 
   * @param handler Callback function to handle data frames / 处理数据帧的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called for each DATA frame received on any stream
   * @details 对在任何流上接收到的每个DATA帧调用
   */
  connection &on_data(on_stream_data_t handler) {
    on_stream_data_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for stream end events
   * @brief 设置流结束事件的回调
   * 
   * @param handler Callback function for stream completion / 流完成的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called when a stream reaches its final state
   * @details 当流达到最终状态时调用
   */
  connection &on_stream_end(on_stream_end_t handler) {
    on_stream_end_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for stream-level errors
   * @brief 设置流级别错误的回调
   * 
   * @param handler Callback function for stream errors / 流错误的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called when RST_STREAM frames are received or stream violations occur
   * @details 接收到RST_STREAM帧或发生流违规时调用
   */
  connection &on_stream_error(on_stream_error_t handler) {
    on_stream_error_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for connection-level errors
   * @brief 设置连接级别错误的回调
   * 
   * @param handler Callback function for connection errors / 连接错误的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called for fatal connection errors that require connection closure
   * @details 对需要关闭连接的致命连接错误调用
   */
  connection &on_connection_error(on_connection_error_t handler) {
    on_connection_error_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for settings negotiation
   * @brief 设置设置协商的回调
   * 
   * @param handler Callback function for settings updates / 设置更新的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called when SETTINGS frames are received from peer
   * @details 从对等方接收到SETTINGS帧时调用
   */
  connection &on_settings(on_settings_t handler) {
    on_settings_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for ping frames
   * @brief 设置ping帧的回调
   * 
   * @param handler Callback function for ping events / ping事件的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called for both ping requests and acknowledgments
   * @details 对ping请求和确认都调用
   */
  connection &on_ping(on_ping_t handler) {
    on_ping_ = std::move(handler);
    return *this;
  }

  /**
   * @brief Set callback for connection shutdown
   * @brief 设置连接关闭的回调
   * 
   * @param handler Callback function for GOAWAY frames / GOAWAY帧的回调函数
   * @return connection& Reference to this connection for method chaining / 此连接的引用以进行方法链接
   * 
   * @details Called when the peer initiates graceful connection shutdown
   * @details 当对等方启动优雅连接关闭时调用
   */
  connection &on_goaway(on_goaway_t handler) {
    on_goaway_ = std::move(handler);
    return *this;
  }

  // =============================================================================
  // Connection Operations / 连接操作
  // Core methods for processing and generating HTTP/2 protocol data
  // 处理和生成HTTP/2协议数据的核心方法
  // =============================================================================

  /**
   * @brief Process incoming HTTP/2 protocol data
   * @brief 处理传入的HTTP/2协议数据
   * 
   * @param data Raw bytes received from network / 从网络接收的原始字节
   * @return std::expected<size_t, v2::h2_error_code> Bytes consumed or error / 消耗的字节数或错误
   * 
   * @details This method handles the complete HTTP/2 protocol processing:
   * @details 此方法处理完整的HTTP/2协议处理：
   * - Connection preface handling (client/server) / 连接前置处理（客户端/服务器）
   * - Frame parsing and validation / 帧解析和验证
   * - HPACK header decompression / HPACK头部解压缩
   * - Stream state management / 流状态管理
   * - Flow control enforcement / 流量控制执行
   * - Callback invocation for parsed events / 解析事件的回调调用
   * 
   * @note This method should be called with all available data from the transport layer
   * @note 应该使用传输层的所有可用数据调用此方法
   * 
   * @example
   * // In your network reading loop:
   * while (auto data = socket.read()) {
   *   auto result = connection.process(data);
   *   if (!result) {
   *     // Handle connection error
   *     break;
   *   }
   *   // Continue processing...
   * }
   */
  std::expected<size_t, v2::h2_error_code>
  process(std::span<const uint8_t> data) {
    if (!preface_sent_ && !is_server_) {
      // Client must send preface first
      output_buffer buffer;
      if (auto result = processor_.generate_connection_preface(buffer);
          !result) {
        return result;
      }
      preface_data_ = buffer.to_string();
      preface_sent_ = true;
    }

    if (!preface_received_) {
      auto result = processor_.process_connection_preface(data);
      if (!result)
        return result;

      if (*result == data.size()) {
        preface_received_ = true;
        return *result;
      }

      auto remaining = data.subspan(*result);
      auto frame_result = processor_.process_frames(remaining);
      if (!frame_result)
        return frame_result;

      return *result + *frame_result;
    }

    return processor_.process_frames(data);
  }

  /**
   * @brief Get connection preface data for transmission (client-side)
   * @brief 获取用于传输的连接前置数据（客户端）
   * 
   * @return std::string_view The HTTP/2 connection preface / HTTP/2连接前置
   * 
   * @details Returns the standard HTTP/2 connection preface that clients must send
   * @details 返回客户端必须发送的标准HTTP/2连接前置
   * before any other frames. This is automatically generated on first use.
   * 在任何其他帧之前。这在首次使用时自动生成。
   */
  std::string_view get_preface() const noexcept { return preface_data_; }

  // =============================================================================
  // Stream Operations / 流操作
  // Methods for sending and managing HTTP/2 streams
  // 发送和管理HTTP/2流的方法
  // =============================================================================

  /**
   * @brief Send HTTP request on a new stream (client-side)
   * @brief 在新流上发送HTTP请求（客户端）
   * 
   * @param req The HTTP request to send / 要发送的HTTP请求
   * @param end_stream Whether to close the stream after sending / 发送后是否关闭流
   * @return std::expected<output_buffer, v2::h2_error_code> Generated frames or error / 生成的帧或错误
   * 
   * @details Converts an HTTP request into HTTP/2 frames:
   * @details 将HTTP请求转换为HTTP/2帧：
   * - Automatically assigns a new odd-numbered stream ID / 自动分配新的奇数流ID
   * - Converts HTTP/1-style headers to HTTP/2 pseudo-headers / 将HTTP/1样式头部转换为HTTP/2伪头部
   * - Applies HPACK compression to headers / 对头部应用HPACK压缩
   * - Sends body as DATA frames if present / 如果存在则将正文作为DATA帧发送
   * - Manages stream state transitions / 管理流状态转换
   * 
   * @example
   * request req;
   * req.method_ = method::get;
   * req.target_ = "/api/data";
   * req.headers_ = {{"host", "api.example.com"}};
   * 
   * auto buffer = client.send_request(req);
   * if (buffer) {
   *   // Send buffer contents to server via socket
   *   socket.write(buffer.value().span());
   * }
   */
  std::expected<output_buffer, v2::h2_error_code>
  send_request(const request &req, bool end_stream = true) {
    uint32_t stream_id = next_stream_id_;
    next_stream_id_ += 2; // Client streams are odd

    output_buffer buffer;

    // Convert request to headers
    std::vector<header> headers;
    headers.emplace_back(":method", method_string(req.method_type));
    headers.emplace_back(":path", req.target);
    headers.emplace_back(":scheme", "https"); // Default to HTTPS

    for (const auto &h : req.headers) {
      headers.push_back(h);
    }

    auto result = processor_.generate_headers_frame(
        stream_id, headers, end_stream && req.body.empty(), true, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }

    // Send body if present
    if (!req.body.empty()) {
      auto body_span = std::span<const uint8_t>(
          reinterpret_cast<const uint8_t *>(req.body.data()),
          req.body.size());
      auto data_result = processor_.generate_data_frame(stream_id, body_span,
                                                        end_stream, buffer);
      if (!data_result) {
        return std::unexpected{data_result.error()};
      }
    }

    return buffer;
  }

  /**
   * @brief Send HTTP response on existing stream (server-side)
   * @brief 在现有流上发送HTTP响应（服务器端）
   * 
   * @param stream_id The stream ID to send response on / 要发送响应的流ID
   * @param resp The HTTP response to send / 要发送的HTTP响应
   * @param end_stream Whether to close the stream after sending / 发送后是否关闭流
   * @return std::expected<output_buffer, v2::h2_error_code> Generated frames or error / 生成的帧或错误
   * 
   * @details Converts an HTTP response into HTTP/2 frames:
   * @details 将HTTP响应转换为HTTP/2帧：
   * - Uses provided stream ID (must be from earlier request) / 使用提供的流ID（必须来自较早的请求）
   * - Converts status code to :status pseudo-header / 将状态码转换为:status伪头部
   * - Applies HPACK compression efficiently / 高效应用HPACK压缩
   * - Handles response body as DATA frames / 将响应正文处理为DATA帧
   * - Respects flow control limits / 遵守流量控制限制
   * 
   * @example
   * // In your request handler:
   * connection.on_request([&](uint32_t stream_id, const request& req, bool end_stream) {
   *   response resp;
   *   resp.status_code_ = status_code::ok;
   *   resp.body_ = R"({"result": "success"})";
   *   
   *   auto buffer = connection.send_response(stream_id, resp);
   *   if (buffer) {
   *     socket.write(buffer.value().span());
   *   }
   * });
   */
  std::expected<output_buffer, v2::h2_error_code>
  send_response(uint32_t stream_id, const response &resp,
                bool end_stream = true) {
    output_buffer buffer;

    // Convert response to headers
    std::vector<header> headers;
    headers.emplace_back(":status",
                         std::to_string(static_cast<int>(resp.status_code)));

    for (const auto &h : resp.headers) {
      headers.push_back(h);
    }

    auto result = processor_.generate_headers_frame(
        stream_id, headers, end_stream && resp.body.empty(), true, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }

    // Send body if present
    if (!resp.body.empty()) {
      auto body_span = std::span<const uint8_t>(
          reinterpret_cast<const uint8_t *>(resp.body.data()),
          resp.body.size());
      auto data_result = processor_.generate_data_frame(stream_id, body_span,
                                                        end_stream, buffer);
      if (!data_result) {
        return std::unexpected{data_result.error()};
      }
    }

    return buffer;
  }

  // Send data frame
  std::expected<output_buffer, v2::h2_error_code>
  send_data(uint32_t stream_id, std::span<const uint8_t> data,
            bool end_stream = false) {
    output_buffer buffer;
    auto result =
        processor_.generate_data_frame(stream_id, data, end_stream, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }
    return buffer;
  }

  std::expected<output_buffer, v2::h2_error_code>
  send_data(uint32_t stream_id, std::string_view data,
            bool end_stream = false) {
    auto span = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t *>(data.data()), data.size());
    return send_data(stream_id, span, end_stream);
  }

  // =============================================================================
  // Control Operations
  // =============================================================================

  // Send settings
  std::expected<output_buffer, v2::h2_error_code>
  send_settings(const std::unordered_map<uint16_t, uint32_t> &settings) {
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
  std::expected<output_buffer, v2::h2_error_code>
  send_ping(std::span<const uint8_t, 8> data, bool ack = false) {
    output_buffer buffer;
    auto result = processor_.generate_ping_frame(data, ack, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }
    return buffer;
  }

  // Send GOAWAY
  std::expected<output_buffer, v2::h2_error_code>
  send_goaway(v2::h2_error_code error_code, std::string_view debug_data = "") {
    output_buffer buffer;
    auto result = processor_.generate_goaway_frame(
        last_processed_stream_id_, error_code, debug_data, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }
    return buffer;
  }

  // Send window update
  std::expected<output_buffer, v2::h2_error_code>
  send_window_update(uint32_t stream_id, uint32_t increment) {
    output_buffer buffer;
    auto result =
        processor_.generate_window_update_frame(stream_id, increment, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }
    return buffer;
  }

  // Send RST_STREAM
  std::expected<output_buffer, v2::h2_error_code>
  send_rst_stream(uint32_t stream_id, v2::h2_error_code error_code) {
    output_buffer buffer;
    auto result =
        processor_.generate_rst_stream_frame(stream_id, error_code, buffer);
    if (!result) {
      return std::unexpected{result.error()};
    }
    return buffer;
  }

  // =============================================================================
  // Configuration and Status
  // =============================================================================

  // Update settings
  void update_settings(const v2::connection_settings &settings) {
    processor_.update_settings(settings);
  }

  const v2::connection_settings &get_settings() const noexcept {
    return processor_.get_settings();
  }

  // Get stream manager
  const v2::stream_manager &get_stream_manager() const noexcept {
    return processor_.get_stream_manager();
  }

  // Get statistics
  const v2::frame_processor::stats &get_stats() const noexcept {
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
    processor_.set_headers_callback([this](uint32_t stream_id,
                                           const std::vector<header> &headers,
                                           bool end_stream, bool end_headers) {
      last_processed_stream_id_ =
          std::max(last_processed_stream_id_, stream_id);

      // Parse headers into request/response
      if (is_server_) {
        // Parse as request
        request req;
        for (const auto &h : headers) {
          if (h.name == ":method") {
            req.method_type = parse_method(h.value);
          } else if (h.name == ":path") {
            req.target = h.value;
          } else if (h.name == ":scheme") {
            // Store scheme if needed
          } else if (h.name == ":authority") {
            req.headers.emplace_back("host", h.value);
          } else if (!h.name.starts_with(':')) {
            req.headers.push_back(h);
          }
        }

        if (on_stream_request_) {
          on_stream_request_(stream_id, req, end_stream);
        }
      } else {
        // Parse as response
        response resp;
        for (const auto &h : headers) {
          if (h.name == ":status") {
            resp.status_code = static_cast<unsigned int>(std::stoi(h.value));
          } else if (!h.name.starts_with(':')) {
            resp.headers.push_back(h);
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

    processor_.set_data_callback([this](uint32_t stream_id,
                                        std::span<const uint8_t> data,
                                        bool end_stream) {
      if (on_stream_data_) {
        on_stream_data_(stream_id, data, end_stream);
      }

      if (end_stream && on_stream_end_) {
        on_stream_end_(stream_id);
      }
    });

    processor_.set_rst_stream_callback(
        [this](uint32_t stream_id, v2::h2_error_code error_code) {
          if (on_stream_error_) {
            on_stream_error_(stream_id, error_code);
          }
        });

    processor_.set_settings_callback(
        [this](const std::unordered_map<uint16_t, uint32_t> &settings,
               bool ack) {
          if (!ack && on_settings_) {
            v2::connection_settings conn_settings = processor_.get_settings();
            for (const auto &[id, value] : settings) {
              conn_settings.apply_setting(id, value);
            }
            on_settings_(conn_settings);
          }
        });

    processor_.set_ping_callback(
        [this](std::span<const uint8_t, 8> data, bool ack) {
          if (on_ping_) {
            on_ping_(data, ack);
          }
        });

    processor_.set_goaway_callback([this](uint32_t last_stream_id,
                                          v2::h2_error_code error_code,
                                          std::string_view debug_data) {
      if (on_goaway_) {
        on_goaway_(last_stream_id, error_code, debug_data);
      }
    });

    processor_.set_connection_error_callback(
        [this](v2::h2_error_code error_code, std::string_view debug_info) {
          if (on_connection_error_) {
            on_connection_error_(error_code, debug_info);
          }
        });
  }

  method parse_method(const std::string &method_str) {
    if (method_str == "GET")
      return method::get;
    if (method_str == "POST")
      return method::post;
    if (method_str == "PUT")
      return method::put;
    if (method_str == "DELETE")
      return method::delete_;
    if (method_str == "HEAD")
      return method::head;
    if (method_str == "OPTIONS")
      return method::options;
    if (method_str == "TRACE")
      return method::trace;
    if (method_str == "CONNECT")
      return method::connect;
    if (method_str == "PATCH")
      return method::patch;
    return method::unknown;
  }

  std::string method_string(method m) {
    switch (m) {
    case method::get:
      return "GET";
    case method::post:
      return "POST";
    case method::put:
      return "PUT";
    case method::delete_:
      return "DELETE";
    case method::head:
      return "HEAD";
    case method::options:
      return "OPTIONS";
    case method::trace:
      return "TRACE";
    case method::connect:
      return "CONNECT";
    case method::patch:
      return "PATCH";
    case method::unknown:
      return "UNKNOWN";
    }
    return "UNKNOWN";
  }
};

// =============================================================================
// Convenience Functions / 便利函数
// Factory functions for creating HTTP/2 connections
// 用于创建HTTP/2连接的工厂函数
// =============================================================================

/**
 * @brief Create a new HTTP/2 client connection
 * @brief 创建新的HTTP/2客户端连接
 * 
 * @return connection A client-configured HTTP/2 connection / 客户端配置的HTTP/2连接
 * 
 * @details Creates a connection configured for client use:
 * @details 创建配置为客户端使用的连接：
 * - Uses odd-numbered stream IDs starting from 1 / 使用从1开始的奇数流ID
 * - Handles connection preface automatically / 自动处理连接前置
 * - Suitable for making HTTP requests / 适用于发出HTTP请求
 * 
 * @example
 * auto client = http2::client()
 *   .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
 *     std::cout << "Response received on stream " << stream_id << std::endl;
 *   });
 */
inline connection client() { return connection(false); }

/**
 * @brief Create a new HTTP/2 server connection
 * @brief 创建新的HTTP/2服务器连接
 * 
 * @return connection A server-configured HTTP/2 connection / 服务器配置的HTTP/2连接
 * 
 * @details Creates a connection configured for server use:
 * @details 创建配置为服务器使用的连接：
 * - Uses even-numbered stream IDs for server push / 使用偶数流ID进行服务器推送
 * - Expects to receive connection preface from client / 期望从客户端接收连接前置
 * - Suitable for handling HTTP requests / 适用于处理HTTP请求
 * 
 * @example
 * auto server = http2::server()
 *   .on_request([](uint32_t stream_id, const request& req, bool end_stream) {
 *     std::cout << "Request received: " << req.target_ << std::endl;
 *   });
 */
inline connection server() { return connection(true); }

} // namespace http2

// =============================================================================
// Unified Convenience Interface / 统一便利接口
// Single entry point for all HTTP parsing functionality
// 所有HTTP解析功能的单一入口点
// =============================================================================

/**
 * @brief Unified HTTP parsing interface providing access to all functionality
 * @brief 提供所有功能访问的统一HTTP解析接口
 * 
 * @details This class serves as a convenient entry point for all HTTP parsing operations:
 * @details 此类作为所有HTTP解析操作的便利入口点：
 * - Static methods for common HTTP/1.x operations / HTTP/1.x常见操作的静态方法
 * - Factory methods for HTTP/2 connections / HTTP/2连接的工厂方法
 * - Utility functions for version and error handling / 版本和错误处理的实用函数
 * - Consistent API across protocol versions / 跨协议版本的一致API
 * 
 * @example
 * // Parse HTTP/1.x request
 * auto req = http_parse::parse_request(data);
 * 
 * // Create HTTP/2 client
 * auto client = http_parse::http2_client();
 * 
 * // Build request using fluent API
 * auto new_req = http_parse::request()
 *   .method(method::get)
 *   .target("/api/users")
 *   .build();
 */
class http_parse {
public:
  // =============================================================================
  // HTTP/1.x Shortcuts / HTTP/1.x快捷方式
  // Direct access to HTTP/1.x parsing and encoding functions
  // 直接访问HTTP/1.x解析和编码函数
  // =============================================================================
  
  /**
   * @brief Parse HTTP/1.x request (unified interface)
   * @brief 解析HTTP/1.x请求（统一接口）
   */
  static auto parse_request(std::string_view data) {
    return http1::parse_request(data);
  }
  
  /**
   * @brief Parse HTTP/1.x response (unified interface)
   * @brief 解析HTTP/1.x响应（统一接口）
   */
  static auto parse_response(std::string_view data) {
    return http1::parse_response(data);
  }
  
  /**
   * @brief Encode HTTP/1.x request (unified interface)
   * @brief 编码HTTP/1.x请求（统一接口）
   */
  static auto encode_request(const request &req) {
    return http1::encode_request(req);
  }
  
  /**
   * @brief Encode HTTP/1.x response (unified interface)
   * @brief 编码HTTP/1.x响应（统一接口）
   */
  static auto encode_response(const response &resp) {
    return http1::encode_response(resp);
  }

  // =============================================================================
  // Builder Shortcuts / 构建器快捷方式
  // Convenient access to fluent API builders
  // 便利访问流畅API构建器
  // =============================================================================
  
  /**
   * @brief Create HTTP request builder (unified interface)
   * @brief 创建HTTP请求构建器（统一接口）
   */
  static auto request() { return http1::request(); }
  
  /**
   * @brief Create HTTP response builder (unified interface)
   * @brief 创建HTTP响应构建器（统一接口）
   */
  static auto response() { return http1::response(); }

  // =============================================================================
  // HTTP/2 Shortcuts / HTTP/2快捷方式
  // Factory methods for HTTP/2 connections
  // HTTP/2连接的工厂方法
  // =============================================================================
  
  /**
   * @brief Create HTTP/2 client connection (unified interface)
   * @brief 创建HTTP/2客户端连接（统一接口）
   */
  static auto http2_client() { return http2::client(); }
  
  /**
   * @brief Create HTTP/2 server connection (unified interface)
   * @brief 创建HTTP/2服务器连接（统一接口）
   */
  static auto http2_server() { return http2::server(); }

  // =============================================================================
  // Utility Functions / 实用函数
  // Helper functions for version and error handling
  // 版本和错误处理的辅助函数
  // =============================================================================
  
  /**
   * @brief Convert HTTP version enum to string representation
   * @brief 将HTTP版本枚举转换为字符串表示
   * 
   * @param v HTTP version enum value / HTTP版本枚举值
   * @return std::string Human-readable version string / 人类可读的版本字符串
   * 
   * @details Provides string representation for logging and debugging:
   * @details 为日志记录和调试提供字符串表示：
   * - HTTP/1.0, HTTP/1.1, HTTP/2.0 for standard versions / 标准版本的HTTP/1.0、HTTP/1.1、HTTP/2.0
   * - AUTO for automatic detection mode / 自动检测模式的AUTO
   * - UNKNOWN for unrecognized versions / 未识别版本的UNKNOWN
   */
  static std::string version_string(version v) {
    switch (v) {
    case version::http_1_0:
      return "HTTP/1.0";
    case version::http_1_1:
      return "HTTP/1.1";
    case version::http_2_0:
      return "HTTP/2.0";
    case version::auto_detect:
      return "AUTO";
    }
    return "UNKNOWN";
  }

  /**
   * @brief Convert error code enum to descriptive string
   * @brief 将错误代码枚举转换为描述性字符串
   * 
   * @param e Error code enum value / 错误代码枚举值
   * @return std::string Human-readable error description / 人类可读的错误描述
   * 
   * @details Provides descriptive error messages for:
   * @details 为以下内容提供描述性错误消息：
   * - Protocol-level errors / 协议级错误
   * - Parsing and validation errors / 解析和验证错误
   * - HTTP/2 specific errors / HTTP/2特定错误
   * - Connection and stream errors / 连接和流错误
   * 
   * @example
   * auto result = http_parse::parse_request(data);
   * if (!result) {
   *   std::cerr << "Parse error: " << http_parse::error_string(result.error()) << std::endl;
   * }
   */
  static std::string error_string(error_code e) {
    switch (e) {
    case error_code::success:
      return "Success";
    case error_code::need_more_data:
      return "Need more data";
    case error_code::protocol_error:
      return "Protocol error";
    case error_code::invalid_method:
      return "Invalid method";
    case error_code::invalid_uri:
      return "Invalid URI";
    case error_code::invalid_version:
      return "Invalid version";
    case error_code::invalid_header:
      return "Invalid header";
    case error_code::invalid_body:
      return "Invalid body";
    case error_code::frame_size_error:
      return "Frame size error";
    case error_code::compression_error:
      return "Compression error";
    case error_code::flow_control_error:
      return "Flow control error";
    case error_code::stream_closed:
      return "Stream closed";
    case error_code::connection_error:
      return "Connection error";
    }
    return "Unknown error";
  }
};

} // namespace co::http