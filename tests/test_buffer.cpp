#include <gtest/gtest.h>
#include "http_parse.hpp"
#include <string>
#include <vector>
#include <span>

using namespace co::http;

class BufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }
    
    void TearDown() override {
        // 每个测试后的清理
    }
};

// =============================================================================
// 输出缓冲区基本功能测试
// =============================================================================

TEST_F(BufferTest, OutputBufferConstruction) {
    // 默认构造
    output_buffer buffer1;
    EXPECT_EQ(buffer1.size(), 0);
    EXPECT_TRUE(buffer1.empty());
    
    // 指定初始容量构造
    output_buffer buffer2(1024);
    EXPECT_EQ(buffer2.size(), 0);
    EXPECT_TRUE(buffer2.empty());
}

TEST_F(BufferTest, OutputBufferAppend) {
    output_buffer buffer;
    
    // 追加字符串
    std::string test_str = "Hello, World!";
    buffer.append(test_str);
    
    EXPECT_EQ(buffer.size(), test_str.size());
    EXPECT_FALSE(buffer.empty());
    
    auto view = buffer.view();
    EXPECT_EQ(view, test_str);
}

TEST_F(BufferTest, OutputBufferAppendSpan) {
    output_buffer buffer;
    
    // 追加span
    std::string test_str = "Hello, HTTP/2!";
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(test_str.data()), test_str.size());
    
    buffer.append(span);
    
    EXPECT_EQ(buffer.size(), test_str.size());
    
    auto result_span = buffer.span();
    EXPECT_EQ(result_span.size(), test_str.size());
    
    std::string result(reinterpret_cast<const char*>(result_span.data()), result_span.size());
    EXPECT_EQ(result, test_str);
}

TEST_F(BufferTest, OutputBufferAppendByte) {
    output_buffer buffer;
    
    // 追加单个字节
    buffer.append_byte(static_cast<uint8_t>('A'));
    buffer.append_byte(static_cast<uint8_t>('B'));
    buffer.append_byte(static_cast<uint8_t>('C'));
    
    EXPECT_EQ(buffer.size(), 3);
    
    auto view = buffer.view();
    EXPECT_EQ(view, "ABC");
}

TEST_F(BufferTest, OutputBufferAppendVector) {
    output_buffer buffer;
    
    // 追加字节向量
    std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    std::span<const uint8_t> data_span(data.data(), data.size());
    buffer.append(data_span);
    
    EXPECT_EQ(buffer.size(), data.size());
    
    auto span = buffer.span();
    EXPECT_EQ(span.size(), data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(span[i], data[i]);
    }
}

TEST_F(BufferTest, OutputBufferMultipleAppends) {
    output_buffer buffer;
    
    // 多次追加
    buffer.append("Hello");
    buffer.append(", ");
    buffer.append("World");
    buffer.append("!");
    
    EXPECT_EQ(buffer.size(), 13);
    
    auto view = buffer.view();
    EXPECT_EQ(view, "Hello, World!");
}

TEST_F(BufferTest, OutputBufferReserve) {
    output_buffer buffer;
    
    // 预留空间
    buffer.reserve(2048);
    
    // 预留后仍然为空
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
}

TEST_F(BufferTest, OutputBufferClear) {
    output_buffer buffer;
    
    // 添加数据
    buffer.append("Test data");
    EXPECT_EQ(buffer.size(), 9);
    EXPECT_FALSE(buffer.empty());
    
    // 清空
    buffer.clear();
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
    
    // 清空后仍可以添加数据
    buffer.append("New data");
    EXPECT_EQ(buffer.size(), 8);
    EXPECT_EQ(buffer.view(), "New data");
}

// =============================================================================
// 输出缓冲区性能测试
// =============================================================================

TEST_F(BufferTest, OutputBufferLargeData) {
    output_buffer buffer;
    
    // 追加大量数据
    const size_t data_size = 1024 * 1024; // 1MB
    std::string large_data(data_size, 'A');
    
    buffer.append(large_data);
    
    EXPECT_EQ(buffer.size(), data_size);
    EXPECT_EQ(buffer.view().size(), data_size);
    EXPECT_EQ(buffer.view().front(), 'A');
    EXPECT_EQ(buffer.view().back(), 'A');
}

TEST_F(BufferTest, OutputBufferManySmallAppends) {
    output_buffer buffer;
    
    // 多次追加小数据
    const int num_appends = 10000;
    const std::string small_data = "x";
    
    for (int i = 0; i < num_appends; ++i) {
        buffer.append(small_data);
    }
    
    EXPECT_EQ(buffer.size(), num_appends);
    EXPECT_EQ(buffer.view().size(), num_appends);
    
    // 验证内容
    auto view = buffer.view();
    for (size_t i = 0; i < view.size(); ++i) {
        EXPECT_EQ(view[i], 'x');
    }
}

// =============================================================================
// 输出缓冲区零拷贝访问测试
// =============================================================================

TEST_F(BufferTest, OutputBufferZeroCopyAccess) {
    output_buffer buffer;
    
    std::string test_data = "Zero-copy test data";
    buffer.append(test_data);
    
    // 通过span访问（零拷贝）
    auto span = buffer.span();
    EXPECT_EQ(span.size(), test_data.size());
    
    // 验证数据内容
    std::string_view view_from_span(reinterpret_cast<const char*>(span.data()), span.size());
    EXPECT_EQ(view_from_span, test_data);
    
    // 通过string_view访问（零拷贝）
    auto view = buffer.view();
    EXPECT_EQ(view, test_data);
    EXPECT_EQ(view.data(), reinterpret_cast<const char*>(span.data()));
}

TEST_F(BufferTest, OutputBufferConstAccess) {
    output_buffer buffer;
    buffer.append("Const access test");
    
    const output_buffer& const_buffer = buffer;
    
    // const访问
    auto const_span = const_buffer.span();
    auto const_view = const_buffer.view();
    
    EXPECT_EQ(const_span.size(), buffer.size());
    EXPECT_EQ(const_view.size(), buffer.size());
    EXPECT_EQ(const_view, "Const access test");
}

// =============================================================================
// 内存管理测试
// =============================================================================

TEST_F(BufferTest, OutputBufferMove) {
    output_buffer buffer1;
    std::string test_data = "Move test data";
    buffer1.append(test_data);
    
    // 移动构造
    output_buffer buffer2 = std::move(buffer1);
    
    EXPECT_EQ(buffer2.view(), test_data);
    EXPECT_EQ(buffer2.size(), test_data.size());
    
    // 原缓冲区应该为空或处于有效但未指定状态
    // 我们不对移动后的状态做特定断言，因为这依赖于实现
}

// =============================================================================
// 边界条件和错误处理测试
// =============================================================================

TEST_F(BufferTest, EmptyBufferOperations) {
    output_buffer empty_buffer;
    
    // 空缓冲区的操作
    EXPECT_EQ(empty_buffer.size(), 0);
    EXPECT_TRUE(empty_buffer.empty());
    EXPECT_EQ(empty_buffer.view(), "");
    
    auto span = empty_buffer.span();
    EXPECT_EQ(span.size(), 0);
    
    // 清空空缓冲区
    empty_buffer.clear();
    EXPECT_EQ(empty_buffer.size(), 0);
}

TEST_F(BufferTest, OutputBufferStringConversion) {
    output_buffer buffer;
    std::string test_data = "String conversion test";
    buffer.append(test_data);
    
    // 转换为string
    std::string result = buffer.to_string();
    EXPECT_EQ(result, test_data);
    
    // 获取string_view
    auto view = buffer.string_view();
    EXPECT_EQ(view, test_data);
}