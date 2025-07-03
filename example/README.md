# HTTP Parse Library 示例程序

本目录包含了HTTP Parse Library的完整示例程序，演示HTTP/1.x和HTTP/2协议的解码、编码和多数据块传输功能。

## 📁 文件结构

```
example/
├── http1_example.cpp           # HTTP/1.x完整示例
├── http2_example.cpp           # HTTP/2完整示例  
├── comprehensive_example.cpp   # 综合示例（多数据块传输）
├── Makefile                    # 编译脚本
└── README.md                   # 本说明文档
```

## 🚀 快速开始

### 编译示例

```bash
# 编译所有示例
make all

# 或者单独编译
make http1_example
make http2_example
make comprehensive_example
```

### 运行示例

```bash
# 运行所有示例
make run

# 或者单独运行
make run-http1
make run-http2  
make run-comprehensive
```

### 检查环境

```bash
make check
```

## 📖 示例详解

### 1. HTTP/1.x 示例 (`http1_example.cpp`)

#### 功能演示：
- ✅ **简单解析** - 一次性解析完整的HTTP请求和响应
- ✅ **流式解析** - 增量解析大型请求，模拟网络分片传输
- ✅ **消息编码** - 构建和编码HTTP请求和响应
- ✅ **高性能缓冲区** - 使用零拷贝缓冲区提升性能
- ✅ **错误处理** - 各种错误情况的检测和处理
- ✅ **完整通信流程** - 客户端-服务器完整交互模拟

#### 核心演示场景：
```cpp
// 1. 简单解析
auto result = http1::parse_request("GET /api/users HTTP/1.1\r\n...");

// 2. 流式解析大文件
http1::request_parser parser;
for (const auto& chunk : data_chunks) {
    parser.parse(chunk, request);
}

// 3. 构建和编码
auto response = http1::response()
    .status(200)
    .header("Content-Type", "application/json")
    .body(json_data);
```

### 2. HTTP/2 示例 (`http2_example.cpp`)

#### 功能演示：
- ✅ **客户端连接** - HTTP/2客户端创建和事件处理
- ✅ **服务器连接** - HTTP/2服务器创建和请求处理
- ✅ **帧处理** - 各种HTTP/2帧类型的生成和解析
- ✅ **流式数据** - 多路复用流的并发处理
- ✅ **完整通信** - 客户端-服务器完整HTTP/2交互

#### 核心演示场景：
```cpp
// 1. 创建HTTP/2客户端
auto client = http2::client()
    .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
        // 处理响应
    })
    .on_data([](uint32_t stream_id, std::span<const uint8_t> data, bool end_stream) {
        // 处理数据
    });

// 2. 发送请求
auto buffer = client.send_request(request);

// 3. 处理接收数据
client.process(incoming_data);
```

### 3. 综合示例 (`comprehensive_example.cpp`)

#### 功能演示：
- ✅ **网络模拟** - 真实网络条件模拟（延迟、丢包）
- ✅ **多数据块传输** - HTTP/1.x大型数据的分片传输
- ✅ **并发流处理** - HTTP/2多流并发传输
- ✅ **性能统计** - 详细的传输性能分析
- ✅ **实际场景** - 模拟真实的应用场景

#### 核心演示场景：
```cpp
// 1. 网络模拟器
NetworkSimulator network(0.05, 10, 100); // 5%丢包率, 10-100ms延迟

// 2. HTTP/1.x分片传输
for (const auto& chunk : request_chunks) {
    network.send_packet(chunk, "客户端->服务器");
}

// 3. HTTP/2并发流
std::vector<RequestInfo> concurrent_requests = {
    {"获取用户", method::get, "/api/users"},
    {"获取产品", method::get, "/api/products"},
    {"创建订单", method::post, "/api/orders"}
};
```

## 🎯 演示重点

### HTTP/1.x 重点演示

#### 1. 流式解析能力
```cpp
// 模拟1000个用户的批量导入请求（约250KB数据）
std::string large_json = generate_bulk_users(1000);
auto request = http1::request()
    .method(method::post)
    .target("/api/users/bulk-import")
    .body(large_json);

// 分512字节块传输
const size_t chunk_size = 512;
for (size_t i = 0; i < encoded_data.size(); i += chunk_size) {
    auto chunk = encoded_data.substr(i, chunk_size);
    parser.parse(chunk, received_request);
}
```

#### 2. 完整请求-响应流程
```cpp
// 客户端: 构建登录请求
auto login_req = http1::request()
    .method(method::post)
    .target("/api/auth/login")
    .body(R"({"username": "user123", "password": "secret456"})");

// 网络: 分片传输
auto chunks = split_data(encoded_request, 50);

// 服务器: 流式接收和处理
http1::request_parser server_parser;
// ... 接收处理逻辑

// 服务器: 生成响应
auto response = http1::response()
    .status(status_code::ok)
    .body(R"({"token": "jwt_token", "expires_in": 3600})");
```

### HTTP/2 重点演示

#### 1. 多路复用能力
```cpp
// 同时发送8个不同的请求
std::vector<RequestInfo> concurrent_requests = {
    {"获取用户列表", method::get, "/api/users"},
    {"获取产品列表", method::get, "/api/products"},
    {"获取订单列表", method::get, "/api/orders"},
    {"慢请求测试", method::get, "/api/slow/test"},
    {"创建新用户", method::post, "/api/users"},
    {"创建新产品", method::post, "/api/products"},
    {"大数据查询", method::get, "/api/analytics/report"},
    {"文件上传", method::post, "/api/files"}
};

// 并发发送，各流独立处理
for (size_t i = 0; i < requests.size(); ++i) {
    uint32_t stream_id = (i + 1) * 2 + 1;
    auto buffer = client.send_request(build_request(requests[i]));
    // 网络发送...
}
```

#### 2. 帧级别操作
```cpp
// 生成各种HTTP/2帧
auto settings_frame = client.send_settings(settings_map);
auto ping_frame = client.send_ping(ping_data);
auto window_frame = client.send_window_update(stream_id, increment);
auto rst_frame = client.send_rst_stream(stream_id, error_code);
auto goaway_frame = client.send_goaway(error_code, debug_info);

// 每个帧都显示十六进制表示
std::cout << "SETTINGS帧: " << to_hex(settings_data) << "\n";
```

#### 3. 流控制演示
```cpp
// 窗口更新
client.send_window_update(0, 32768);      // 连接级别
client.send_window_update(stream_id, 1024); // 流级别

// 流错误处理
client.on_stream_error([](uint32_t stream_id, v2::h2_error_code error) {
    std::cout << "流错误: " << stream_id << " -> " << static_cast<int>(error);
});
```

## 📊 性能展示

### 网络模拟统计
```
📦📡 [客户端->服务器] 发送数据包 (512 字节, 延迟 23ms)
📦📡 [客户端->服务器] 发送数据包 (512 字节, 延迟 45ms)
📦❌ [客户端->服务器] 数据包丢失 (512 字节)
📦✅ 接收数据包 (512 字节)
```

### HTTP/1.x 性能统计
```
📊 接收统计:
   - 总字节数: 245760
   - 请求方法: 1 (POST)
   - 请求路径: /api/users/bulk-import
   - 请求体大小: 245234 字节
   - 传输耗时: 1247ms
```

### HTTP/2 并发统计
```
📊 总体统计:
   - 总请求数: 8
   - 完成请求数: 8  
   - 总耗时: 256ms
   - 平均每请求: 32ms

📊 各流详细统计:
   - Stream 1 (获取用户列表): 45ms ✅
   - Stream 3 (获取产品列表): 38ms ✅
   - Stream 5 (获取订单列表): 52ms ✅
   - Stream 7 (慢请求测试): 247ms ✅
   - Stream 9 (创建新用户): 67ms ✅
   - Stream 11 (创建新产品): 43ms ✅
   - Stream 13 (大数据查询): 89ms ✅
   - Stream 15 (文件上传): 78ms ✅
```

## 🛠 编译要求

- **C++23兼容编译器**
  - GCC 13+ 
  - Clang 16+
  - MSVC 19.37+
- **依赖库**
  - pthread (多线程支持)
- **系统要求**
  - Linux / macOS / Windows

## 🔧 自定义修改

### 修改网络条件
```cpp
// 在comprehensive_example.cpp中修改
NetworkSimulator network(
    0.02,    // 丢包率 (2%)
    5,       // 最小延迟 (5ms)
    30       // 最大延迟 (30ms)
);
```

### 修改数据大小
```cpp
// 修改chunk_size调整分片大小
const size_t chunk_size = 1024; // 1KB块

// 修改用户数量调整数据大小
for (int i = 1; i <= 5000; ++i) { // 5000个用户
    // 生成用户数据...
}
```

### 添加新的请求类型
```cpp
// 在requests数组中添加新请求
std::vector<RequestInfo> requests = {
    // 现有请求...
    {"自定义API", method::get, "/api/custom/endpoint", ""},
    {"大文件上传", method::post, "/api/upload/large", large_file_data},
};
```

## 🎓 学习要点

通过这些示例，你将学会：

1. **HTTP/1.x协议处理**
   - 请求/响应的解析和生成
   - 流式处理大型数据
   - 错误检测和恢复

2. **HTTP/2协议特性**
   - 多路复用的实现原理
   - 帧级别的协议操作
   - 流控制和优先级管理

3. **网络编程最佳实践**
   - 分片传输处理
   - 异步事件处理
   - 性能监控和优化

4. **实际应用场景**
   - RESTful API交互
   - 大文件传输
   - 实时数据流处理

## 🚀 扩展建议

基于这些示例，你可以进一步：

1. **集成网络库** - 与实际的socket或网络库集成
2. **添加TLS支持** - 实现HTTPS/HTTP2 over TLS
3. **性能基准测试** - 与其他HTTP库对比性能
4. **压力测试** - 测试高并发场景下的表现
5. **生产环境适配** - 添加日志、监控、容错机制

这些示例为你提供了一个完整的起点，展示了HTTP Parse Library的强大功能和优雅设计！