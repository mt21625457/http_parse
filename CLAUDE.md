# HTTP解析库开发指南

## 项目概述
- 现代C++23 HTTP/1.x和HTTP/2协议解析库
- 纯头文件实现，支持零拷贝高性能解析
- 完整的RFC7540/7541/9113标准兼容性

## 构建命令
- `./build_libcxx.sh`: 构建项目（使用libc++，默认Release模式）
- `./build_libcxx.sh --debug`: 调试模式构建并运行测试
- `./build_libcxx.sh --test`: 构建后运行测试
- `./build_libcxx.sh --clean`: 清理构建目录
- `./build_libcxx.sh --jobs=8`: 使用8个并行任务构建
- `cmake --preset cxx23`: 使用C++23预设配置（可选）
- `cmake --build --preset cxx23`: 构建C++23版本（可选）
- `ctest --preset cxx23`: 运行测试（可选）

## 项目结构
- `include/`: 头文件目录
  - `http_parse.hpp`: 主头文件
  - `http_parse/`: 核心实现
    - `v1/`: HTTP/1.x实现
    - `v2/`: HTTP/2实现
    - `detail/`: 实现细节
- `example/`: 示例代码
  - `http1_example.cpp`: HTTP/1.x示例
  - `http2_example.cpp`: HTTP/2示例
  - `comprehensive_example.cpp`: 综合示例
- `tests/`: 单元测试模块
- `cmake/`: CMake配置文件

## 代码风格
- 遵循Google C++编码规范
- 函数返回值不要返回大对象，尽量返回值简单
- 使用std::expected作为函数返回值处理错误
- 使用std::error_code标识错误类型
- 使用标准库memory_resource进行内存管理
- 不要使用异常，采用std::expected错误处理
- 使用C++23等高版本特性（std::span, std::expected）
- 使用std::string_view进行零拷贝字符串操作
- 优先使用constexpr和noexcept
- 采用RAII原则管理资源

## 工作流程
- 在完成一系列代码更改后务必运行 `./build_libcxx.sh` 进行编译检查
- 为了确保代码改动没有问题，必须运行整个单元测试：`./build_libcxx.sh --test`
- 调试时使用：`./build_libcxx.sh --debug` （自动运行测试）
- 清理重新构建：`./build_libcxx.sh --clean`
- 使用 `ctest --preset cxx23` 运行测试套件（可选）
- 运行性能测试和压力测试验证性能指标

## 构建优化
- **并行构建**：自动检测CPU核心数进行并行编译
- **编译器优化**：Release模式使用-O3和-march=native优化
- **链接时优化**：启用LTO (Link Time Optimization)
- **头文件库**：纯头文件实现，无需链接库文件
- **CMake模块化**：支持作为子项目或独立库使用

## 单元测试
- `tests/` 目录下为googletest单元测试模块
- 对所有的接口进行单元测试、压力测试、性能测试
- 测试覆盖：
  - `test_http1_parser.cpp`: HTTP/1.x解析器测试
  - `test_http2_frame_processor.cpp`: HTTP/2帧处理器测试
  - `test_hpack.cpp`: HPACK压缩算法测试
  - `benchmark_tests.cpp`: 性能基准测试
  - `stress_tests.cpp`: 压力测试
- 运行测试：`./cmake-build-release/http_parse_tests`

## API设计原则
- 提供统一的高级API接口，隐藏协议版本差异
- 支持流式解析和一次性解析两种模式
- 零拷贝设计，避免不必要的内存分配和复制
- 使用Builder模式构建HTTP消息对象
- 事件驱动的HTTP/2连接管理
- 线程安全的并发操作支持

## 关键特性
- **HTTP/1.x支持**：完整的RFC7230/7231标准实现
- **HTTP/2支持**：RFC7540/7541/9113标准兼容
- **HPACK压缩**：高效的头部压缩算法
- **流复用**：HTTP/2多路复用和流控制
- **零拷贝**：std::span和std::string_view实现
- **错误处理**：std::expected无异常错误处理
- **性能优化**：内存池和缓冲区复用

## 使用指南
### 基础用法
```cpp
#include "http_parse.hpp"
using namespace co::http;

// HTTP/1.x解析
auto result = http1::parse_request(data);
if (result) {
    auto& req = *result;
    // 处理请求
}

// HTTP/2连接
auto client = http2::client()
    .on_response([](uint32_t stream_id, const response& resp, bool end_stream) {
        // 处理响应
    });
```

### 流式解析
```cpp
http1::request_parser parser;
request req;
while (auto chunk = read_data()) {
    auto result = parser.parse(chunk, req);
    if (parser.is_complete()) break;
}
```

## 调试和性能分析
- 使用 `-DCMAKE_BUILD_TYPE=Debug` 进行调试构建
- 启用详细日志：`-DHTTP_PARSE_ENABLE_LOGGING=ON`
- 性能分析：运行 `benchmark_tests.cpp` 中的基准测试
- 内存分析：使用AddressSanitizer和Valgrind检查内存泄漏

## 常见问题
- **编译错误**：确保使用C++23兼容的编译器
- **链接错误**：检查libc++库路径设置
- **解析失败**：使用详细错误信息进行调试
- **性能问题**：启用Release构建和编译器优化

## 扩展开发
- 新增HTTP方法：修改 `core.hpp` 中的method枚举
- 自定义错误处理：扩展 `error_code` 枚举
- 添加新的HTTP/2帧类型：扩展 `v2/types.hpp`
- 性能优化：使用自定义内存分配器