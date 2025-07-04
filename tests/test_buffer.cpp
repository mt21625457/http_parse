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
    EXPECT_GT(buffer1.capacity(), 0);
    
    // 指定初始容量构造
    output_buffer buffer2(1024);
    EXPECT_EQ(buffer2.size(), 0);
    EXPECT_TRUE(buffer2.empty());
    EXPECT_GE(buffer2.capacity(), 1024);
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
    buffer.append(static_cast<uint8_t>('A'));
    buffer.append(static_cast<uint8_t>('B'));
    buffer.append(static_cast<uint8_t>('C'));
    
    EXPECT_EQ(buffer.size(), 3);
    
    auto view = buffer.view();
    EXPECT_EQ(view, "ABC");
}

TEST_F(BufferTest, OutputBufferAppendVector) {
    output_buffer buffer;
    
    // 追加字节向量
    std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    buffer.append(data);
    
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
    EXPECT_GE(buffer.capacity(), 2048);
    
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

TEST_F(BufferTest, OutputBufferGrowth) {
    output_buffer buffer;
    
    // 测试缓冲区增长
    size_t previous_capacity = buffer.capacity();
    
    // 逐步增加数据直到容量增长
    std::string data(100, 'X');
    
    while (buffer.capacity() == previous_capacity) {
        buffer.append(data);
    }
    
    // 容量应该增长
    EXPECT_GT(buffer.capacity(), previous_capacity);
    EXPECT_GT(buffer.size(), 0);
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
// 输入缓冲区测试
// =============================================================================

TEST_F(BufferTest, InputBufferConstruction) {
    std::string test_data = "Input buffer test data";
    
    // 从string构造
    input_buffer buffer1(test_data);
    EXPECT_EQ(buffer1.size(), test_data.size());
    EXPECT_EQ(buffer1.remaining(), test_data.size());
    EXPECT_FALSE(buffer1.empty());
    EXPECT_FALSE(buffer1.eof());
    
    // 从span构造
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(test_data.data()), test_data.size());
    input_buffer buffer2(span);
    EXPECT_EQ(buffer2.size(), test_data.size());
    EXPECT_EQ(buffer2.remaining(), test_data.size());
}

TEST_F(BufferTest, InputBufferRead) {
    std::string test_data = "ABCDEFGHIJ";
    input_buffer buffer(test_data);
    
    // 读取单个字节
    auto byte1 = buffer.read_byte();
    ASSERT_TRUE(byte1.has_value());
    EXPECT_EQ(byte1.value(), 'A');
    EXPECT_EQ(buffer.remaining(), 9);
    
    // 读取多个字节
    auto bytes = buffer.read_bytes(3);
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes.value().size(), 3);
    EXPECT_EQ(bytes.value()[0], 'B');
    EXPECT_EQ(bytes.value()[1], 'C');
    EXPECT_EQ(bytes.value()[2], 'D');
    EXPECT_EQ(buffer.remaining(), 6);
    
    // 读取字符串
    auto str = buffer.read_string(4);
    ASSERT_TRUE(str.has_value());
    EXPECT_EQ(str.value(), "EFGH");
    EXPECT_EQ(buffer.remaining(), 2);
}

TEST_F(BufferTest, InputBufferPeek) {
    std::string test_data = "ABCDEFGH";
    input_buffer buffer(test_data);
    
    // 窥视不改变位置
    auto byte1 = buffer.peek_byte();
    ASSERT_TRUE(byte1.has_value());
    EXPECT_EQ(byte1.value(), 'A');
    EXPECT_EQ(buffer.remaining(), 8); // 位置未改变
    
    // 窥视多个字节
    auto bytes = buffer.peek_bytes(4);
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes.value().size(), 4);
    EXPECT_EQ(bytes.value()[0], 'A');
    EXPECT_EQ(bytes.value()[3], 'D');
    EXPECT_EQ(buffer.remaining(), 8); // 位置未改变
    
    // 实际读取应该返回相同数据
    auto actual_bytes = buffer.read_bytes(4);
    ASSERT_TRUE(actual_bytes.has_value());
    EXPECT_EQ(actual_bytes.value(), bytes.value());
    EXPECT_EQ(buffer.remaining(), 4);
}

TEST_F(BufferTest, InputBufferSkip) {
    std::string test_data = "0123456789";
    input_buffer buffer(test_data);
    
    // 跳过字节
    auto result = buffer.skip(3);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer.remaining(), 7);
    
    // 读取下一个字节应该是'3'
    auto byte = buffer.read_byte();
    ASSERT_TRUE(byte.has_value());
    EXPECT_EQ(byte.value(), '3');
    
    // 跳过过多字节
    auto over_skip = buffer.skip(100);
    EXPECT_FALSE(over_skip.has_value());
}

TEST_F(BufferTest, InputBufferSeek) {
    std::string test_data = "ABCDEFGHIJ";
    input_buffer buffer(test_data);
    
    // 前进到位置5
    buffer.seek(5);
    EXPECT_EQ(buffer.position(), 5);
    EXPECT_EQ(buffer.remaining(), 5);
    
    auto byte = buffer.read_byte();
    ASSERT_TRUE(byte.has_value());
    EXPECT_EQ(byte.value(), 'F'); // 索引5是'F'
    
    // 回到开始
    buffer.seek(0);
    EXPECT_EQ(buffer.position(), 0);
    EXPECT_EQ(buffer.remaining(), 10);
    
    byte = buffer.read_byte();
    ASSERT_TRUE(byte.has_value());
    EXPECT_EQ(byte.value(), 'A');
}

TEST_F(BufferTest, InputBufferBoundaryConditions) {
    std::string test_data = "ABC";
    input_buffer buffer(test_data);
    
    // 读取所有数据
    auto all_data = buffer.read_string(3);
    ASSERT_TRUE(all_data.has_value());
    EXPECT_EQ(all_data.value(), "ABC");
    EXPECT_EQ(buffer.remaining(), 0);
    EXPECT_TRUE(buffer.eof());
    
    // 尝试再读取应该失败
    auto extra_byte = buffer.read_byte();
    EXPECT_FALSE(extra_byte.has_value());
    
    auto extra_bytes = buffer.read_bytes(1);
    EXPECT_FALSE(extra_bytes.has_value());
}

// =============================================================================
// 环形缓冲区测试
// =============================================================================

TEST_F(BufferTest, RingBufferConstruction) {
    ring_buffer buffer(1024);
    
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 1024);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.available_write(), 1024);
    EXPECT_EQ(buffer.available_read(), 0);
}

TEST_F(BufferTest, RingBufferWriteRead) {
    ring_buffer buffer(10);
    
    // 写入数据
    std::string test_data = "Hello";
    auto write_result = buffer.write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(test_data.data()), test_data.size()));
    
    EXPECT_TRUE(write_result.has_value());
    EXPECT_EQ(write_result.value(), test_data.size());
    EXPECT_EQ(buffer.size(), test_data.size());
    EXPECT_EQ(buffer.available_write(), 10 - test_data.size());
    
    // 读取数据
    std::vector<uint8_t> read_buffer(test_data.size());
    auto read_result = buffer.read(std::span<uint8_t>(read_buffer));
    
    EXPECT_TRUE(read_result.has_value());
    EXPECT_EQ(read_result.value(), test_data.size());
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
    
    // 验证读取的数据
    std::string read_string(reinterpret_cast<char*>(read_buffer.data()), read_buffer.size());
    EXPECT_EQ(read_string, test_data);
}

TEST_F(BufferTest, RingBufferWrapAround) {
    ring_buffer buffer(8);
    
    // 写入接近满的数据
    std::string data1 = "ABCDEF"; // 6字节
    buffer.write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data1.data()), data1.size()));
    
    // 读取一部分
    std::vector<uint8_t> temp(3);
    buffer.read(std::span<uint8_t>(temp));
    // 现在有3字节在缓冲区中，5字节可用
    
    // 写入更多数据，会发生环绕
    std::string data2 = "GHIJK"; // 5字节
    auto write_result = buffer.write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data2.data()), data2.size()));
    
    EXPECT_TRUE(write_result.has_value());
    EXPECT_EQ(write_result.value(), data2.size());
    EXPECT_EQ(buffer.size(), 8); // 应该满了
    EXPECT_TRUE(buffer.full());
    
    // 读取所有数据
    std::vector<uint8_t> all_data(8);
    auto read_result = buffer.read(std::span<uint8_t>(all_data));
    
    EXPECT_TRUE(read_result.has_value());
    EXPECT_EQ(read_result.value(), 8);
    
    std::string result(reinterpret_cast<char*>(all_data.data()), all_data.size());
    EXPECT_EQ(result, "DEFGHIJK"); // 前3个字节已经被读取了
}

TEST_F(BufferTest, RingBufferOverflow) {
    ring_buffer buffer(5);
    
    // 写入超过容量的数据
    std::string large_data = "ABCDEFGHIJ"; // 10字节，超过5字节容量
    auto write_result = buffer.write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(large_data.data()), large_data.size()));
    
    EXPECT_TRUE(write_result.has_value());
    EXPECT_EQ(write_result.value(), 5); // 只能写入5字节
    EXPECT_TRUE(buffer.full());
    
    // 读取数据验证
    std::vector<uint8_t> read_data(5);
    auto read_result = buffer.read(std::span<uint8_t>(read_data));
    
    EXPECT_TRUE(read_result.has_value());
    EXPECT_EQ(read_result.value(), 5);
    
    std::string result(reinterpret_cast<char*>(read_data.data()), read_data.size());
    EXPECT_EQ(result, "ABCDE");
}

// =============================================================================
// 缓冲区实用函数测试
// =============================================================================

TEST_F(BufferTest, BufferUtilityFunctions) {
    // 测试字节序转换
    uint32_t value = 0x12345678;
    uint32_t be_value = buffer_utils::host_to_be32(value);
    uint32_t le_value = buffer_utils::host_to_le32(value);
    
    EXPECT_EQ(buffer_utils::be32_to_host(be_value), value);
    EXPECT_EQ(buffer_utils::le32_to_host(le_value), value);
    
    // 测试16位转换
    uint16_t value16 = 0x1234;
    uint16_t be_value16 = buffer_utils::host_to_be16(value16);
    uint16_t le_value16 = buffer_utils::host_to_le16(value16);
    
    EXPECT_EQ(buffer_utils::be16_to_host(be_value16), value16);
    EXPECT_EQ(buffer_utils::le16_to_host(le_value16), value16);
}

TEST_F(BufferTest, BufferCopy) {
    std::string source_data = "Copy test data";
    output_buffer source;
    source.append(source_data);
    
    // 复制缓冲区
    output_buffer copy = buffer_utils::copy_buffer(source);
    
    EXPECT_EQ(copy.size(), source.size());
    EXPECT_EQ(copy.view(), source.view());
    
    // 修改原缓冲区不应影响复制
    source.append(" modified");
    EXPECT_NE(copy.view(), source.view());
    EXPECT_EQ(copy.view(), source_data);
}

TEST_F(BufferTest, BufferConcat) {
    output_buffer buffer1;
    buffer1.append("First ");
    
    output_buffer buffer2;
    buffer2.append("Second ");
    
    output_buffer buffer3;
    buffer3.append("Third");
    
    // 连接缓冲区
    std::vector<std::reference_wrapper<const output_buffer>> buffers = {buffer1, buffer2, buffer3};
    auto result = buffer_utils::concat_buffers(buffers);
    
    EXPECT_EQ(result.view(), "First Second Third");
    EXPECT_EQ(result.size(), buffer1.size() + buffer2.size() + buffer3.size());
}

// =============================================================================
// 内存管理测试
// =============================================================================

TEST_F(BufferTest, BufferMemoryManagement) {
    output_buffer buffer;
    
    // 获取初始容量
    size_t initial_capacity = buffer.capacity();
    
    // 添加数据直到容量增长
    std::string chunk(100, 'X');
    while (buffer.capacity() == initial_capacity) {
        buffer.append(chunk);
    }
    
    // 验证容量增长
    EXPECT_GT(buffer.capacity(), initial_capacity);
    
    // 释放多余内存
    buffer.shrink_to_fit();
    
    // 容量应该接近实际使用的大小
    EXPECT_LE(buffer.capacity(), buffer.size() * 2); // 允许一些开销
}

TEST_F(BufferTest, BufferMove) {
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

TEST_F(BufferTest, LargeBufferOperations) {
    const size_t large_size = 10 * 1024 * 1024; // 10MB
    
    output_buffer buffer;
    buffer.reserve(large_size);
    
    // 添加大量数据
    std::string chunk(1024, 'L');
    for (size_t i = 0; i < large_size / 1024; ++i) {
        buffer.append(chunk);
    }
    
    EXPECT_EQ(buffer.size(), large_size);
    EXPECT_GE(buffer.capacity(), large_size);
    
    // 验证数据完整性
    auto view = buffer.view();
    EXPECT_EQ(view.size(), large_size);
    EXPECT_EQ(view.front(), 'L');
    EXPECT_EQ(view.back(), 'L');
}