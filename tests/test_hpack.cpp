#include <gtest/gtest.h>
#include "http_parse.hpp"
#include "http_parse/detail/hpack.hpp"
#include <string>
#include <vector>
#include <array>

using namespace co::http::detail;

class HpackTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
        encoder = std::make_unique<hpack_encoder>();
        decoder = std::make_unique<hpack_decoder>();
    }
    
    void TearDown() override {
        // 每个测试后的清理
        encoder.reset();
        decoder.reset();
    }
    
    std::unique_ptr<hpack_encoder> encoder;
    std::unique_ptr<hpack_decoder> decoder;
    
    // 辅助函数：验证头部字段
    void VerifyHeaderField(const header_field& field, const std::string& name, const std::string& value) {
        EXPECT_EQ(field.name, name);
        EXPECT_EQ(field.value, value);
    }
    
    // 辅助函数：创建测试头部列表
    std::vector<header_field> CreateTestHeaders() {
        return {
            {":method", "GET"},
            {":scheme", "https"},
            {":path", "/"},
            {":authority", "www.example.com"},
            {"user-agent", "Mozilla/5.0"},
            {"accept", "text/html,application/xhtml+xml"}
        };
    }
};

// =============================================================================
// HPACK 静态表测试
// =============================================================================

TEST_F(HpackTest, StaticTableLookup) {
    // 测试RFC 7541附录B中的静态表条目
    
    // Index 1: :authority
    auto entry1 = hpack_static_table::get_entry(1);
    ASSERT_TRUE(entry1.has_value());
    EXPECT_EQ(entry1.value().name, ":authority");
    EXPECT_EQ(entry1.value().value, "");
    
    // Index 2: :method GET
    auto entry2 = hpack_static_table::get_entry(2);
    ASSERT_TRUE(entry2.has_value());
    EXPECT_EQ(entry2.value().name, ":method");
    EXPECT_EQ(entry2.value().value, "GET");
    
    // Index 3: :method POST
    auto entry3 = hpack_static_table::get_entry(3);
    ASSERT_TRUE(entry3.has_value());
    EXPECT_EQ(entry3.value().name, ":method");
    EXPECT_EQ(entry3.value().value, "POST");
    
    // Index 8: :status 200
    auto entry8 = hpack_static_table::get_entry(8);
    ASSERT_TRUE(entry8.has_value());
    EXPECT_EQ(entry8.value().name, ":status");
    EXPECT_EQ(entry8.value().value, "200");
    
    // 无效索引
    auto invalid = hpack_static_table::get_entry(0);
    EXPECT_FALSE(invalid.has_value());
    
    auto out_of_range = hpack_static_table::get_entry(62);
    EXPECT_FALSE(out_of_range.has_value());
}

TEST_F(HpackTest, StaticTableSearch) {
    // 测试在静态表中查找头部
    
    // 查找完全匹配
    auto index1 = hpack_static_table::find_entry(":method", "GET");
    EXPECT_EQ(index1, 2);
    
    auto index2 = hpack_static_table::find_entry(":method", "POST");
    EXPECT_EQ(index2, 3);
    
    auto index3 = hpack_static_table::find_entry(":status", "200");
    EXPECT_EQ(index3, 8);
    
    // 查找只匹配名称
    auto index4 = hpack_static_table::find_entry(":authority", "www.example.com");
    EXPECT_EQ(index4, 1); // 只匹配名称，不匹配值
    
    // 查找不存在的条目
    auto index5 = hpack_static_table::find_entry("custom-header", "value");
    EXPECT_EQ(index5, 0); // 未找到
}

// =============================================================================
// HPACK 动态表测试
// =============================================================================

TEST_F(HpackTest, DynamicTableOperations) {
    hpack_dynamic_table table(4096); // 4KB限制
    
    // 添加条目
    header_field field1{"custom-header", "custom-value"};
    table.add_entry(field1);
    
    EXPECT_EQ(table.size(), 1);
    EXPECT_GT(table.get_used_size(), 0);
    
    // 获取条目（动态表索引从62开始）
    auto entry = table.get_entry(62);
    ASSERT_TRUE(entry.has_value());
    VerifyHeaderField(entry.value(), "custom-header", "custom-value");
    
    // 添加更多条目
    table.add_entry({"another-header", "another-value"});
    table.add_entry({"third-header", "third-value"});
    
    EXPECT_EQ(table.size(), 3);
    
    // 验证FIFO顺序（最新的条目索引最小）
    auto newest = table.get_entry(62);
    ASSERT_TRUE(newest.has_value());
    VerifyHeaderField(newest.value(), "third-header", "third-value");
    
    auto oldest = table.get_entry(64);
    ASSERT_TRUE(oldest.has_value());
    VerifyHeaderField(oldest.value(), "custom-header", "custom-value");
}

TEST_F(HpackTest, DynamicTableEviction) {
    hpack_dynamic_table table(100); // 小的表大小以强制驱逐
    
    // 添加大的条目以触发驱逐
    table.add_entry({"very-long-header-name", "very-long-header-value-that-should-cause-eviction"});
    EXPECT_GT(table.size(), 0);
    
    size_t initial_size = table.size();
    
    // 添加更多条目
    table.add_entry({"header2", "value2"});
    table.add_entry({"header3", "value3"});
    table.add_entry({"header4", "value4"});
    
    // 表大小应该受到限制
    EXPECT_LE(table.get_used_size(), 100);
    
    // 老的条目应该被驱逐
    auto old_entry = table.get_entry(62 + table.size() - 1);
    if (old_entry.has_value()) {
        EXPECT_NE(old_entry.value().name, "very-long-header-name");
    }
}

TEST_F(HpackTest, DynamicTableSizeUpdate) {
    hpack_dynamic_table table(4096);
    
    // 添加一些条目
    table.add_entry({"header1", "value1"});
    table.add_entry({"header2", "value2"});
    table.add_entry({"header3", "value3"});
    
    size_t original_size = table.size();
    EXPECT_EQ(original_size, 3);
    
    // 缩小表大小
    table.set_max_size(50);
    EXPECT_LE(table.get_used_size(), 50);
    EXPECT_LE(table.size(), original_size);
    
    // 扩大表大小
    table.set_max_size(8192);
    EXPECT_LE(table.get_used_size(), 8192);
}

// =============================================================================
// HPACK 编码器测试
// =============================================================================

TEST_F(HpackTest, EncodeIndexedHeaderField) {
    // 测试索引头部字段编码（RFC 7541 6.1）
    
    std::vector<header_field> headers = {{":method", "GET"}};
    output_buffer buffer;
    
    auto result = encoder->encode(headers, buffer);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);
    
    auto encoded = buffer.span();
    ASSERT_GE(encoded.size(), 1);
    
    // :method GET 在静态表中的索引是2
    // 索引头部字段：1xxxxxxx，其中xxxxxxx是索引值
    EXPECT_EQ(encoded[0], 0x82); // 10000010 = 130
}

TEST_F(HpackTest, EncodeLiteralHeaderField) {
    // 测试字面头部字段编码
    
    std::vector<header_field> headers = {{"custom-header", "custom-value"}};
    output_buffer buffer;
    
    auto result = encoder->encode(headers, buffer);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);
    
    auto encoded = buffer.span();
    ASSERT_GT(encoded.size(), 1);
    
    // 字面头部字段，增量索引：01xxxxxx
    EXPECT_EQ(encoded[0] & 0xC0, 0x40); // 检查前两位是01
}

TEST_F(HpackTest, EncodeMultipleHeaders) {
    auto headers = CreateTestHeaders();
    output_buffer buffer;
    
    auto result = encoder->encode(headers, buffer);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);
    
    auto encoded = buffer.span();
    EXPECT_GT(encoded.size(), headers.size()); // 至少每个头部一个字节
}

TEST_F(HpackTest, EncodeWithHuffmanCoding) {
    // 启用Huffman编码
    encoder->set_huffman_encoding(true);
    
    std::vector<header_field> headers = {{"user-agent", "Mozilla/5.0 (compatible; bot)"}};
    output_buffer buffer;
    
    auto result = encoder->encode(headers, buffer);
    ASSERT_TRUE(result.has_value());
    
    // 禁用Huffman编码再编码一次
    encoder->set_huffman_encoding(false);
    output_buffer buffer_no_huffman;
    auto result_no_huffman = encoder->encode(headers, buffer_no_huffman);
    ASSERT_TRUE(result_no_huffman.has_value());
    
    // Huffman编码通常会产生更小的输出
    EXPECT_LE(buffer.size(), buffer_no_huffman.size());
}

// =============================================================================
// HPACK 解码器测试
// =============================================================================

TEST_F(HpackTest, DecodeIndexedHeaderField) {
    // 手动构造索引头部字段
    std::vector<uint8_t> encoded = {0x82}; // :method GET (index 2)
    
    auto result = decoder->decode(std::span<const uint8_t>(encoded));
    ASSERT_TRUE(result.has_value());
    
    const auto& headers = result.value();
    ASSERT_EQ(headers.size(), 1);
    VerifyHeaderField(headers[0], ":method", "GET");
}

TEST_F(HpackTest, DecodeLiteralHeaderField) {
    // 构造字面头部字段：增量索引
    std::vector<uint8_t> encoded = {
        0x40, // 字面头部字段，增量索引，名称索引0（字面名称）
        0x0a, // 名称长度：10
        'c', 'u', 's', 't', 'o', 'm', '-', 'k', 'e', 'y', // 名称
        0x0d, // 值长度：13
        'c', 'u', 's', 't', 'o', 'm', '-', 'h', 'e', 'a', 'd', 'e', 'r' // 值
    };
    
    auto result = decoder->decode(std::span<const uint8_t>(encoded));
    ASSERT_TRUE(result.has_value());
    
    const auto& headers = result.value();
    ASSERT_EQ(headers.size(), 1);
    VerifyHeaderField(headers[0], "custom-key", "custom-header");
}

TEST_F(HpackTest, DecodeMultipleHeaders) {
    // 使用编码器生成测试数据
    auto headers = CreateTestHeaders();
    output_buffer buffer;
    
    auto encode_result = encoder->encode(headers, buffer);
    ASSERT_TRUE(encode_result.has_value());
    
    // 解码
    auto decode_result = decoder->decode(buffer.span());
    ASSERT_TRUE(decode_result.has_value());
    
    const auto& decoded_headers = decode_result.value();
    ASSERT_EQ(decoded_headers.size(), headers.size());
    
    // 验证每个头部
    for (size_t i = 0; i < headers.size(); ++i) {
        VerifyHeaderField(decoded_headers[i], headers[i].name, headers[i].value);
    }
}

// =============================================================================
// HPACK 往返测试
// =============================================================================

TEST_F(HpackTest, EncodeDecodeRoundTrip) {
    auto original_headers = CreateTestHeaders();
    
    // 编码
    output_buffer buffer;
    auto encode_result = encoder->encode(original_headers, buffer);
    ASSERT_TRUE(encode_result.has_value());
    
    // 解码
    auto decode_result = decoder->decode(buffer.span());
    ASSERT_TRUE(decode_result.has_value());
    
    const auto& decoded_headers = decode_result.value();
    
    // 验证往返后头部完全相同
    ASSERT_EQ(decoded_headers.size(), original_headers.size());
    for (size_t i = 0; i < original_headers.size(); ++i) {
        VerifyHeaderField(decoded_headers[i], original_headers[i].name, original_headers[i].value);
    }
}

TEST_F(HpackTest, MultipleRoundTrips) {
    // 多次往返，测试动态表状态的正确性
    
    std::vector<std::vector<header_field>> test_cases = {
        {
            {":method", "GET"},
            {":scheme", "https"},
            {":path", "/"},
            {":authority", "www.example.com"}
        },
        {
            {":method", "POST"},
            {":scheme", "https"},
            {":path", "/api/data"},
            {":authority", "api.example.com"},
            {"content-type", "application/json"}
        },
        {
            {":method", "GET"},
            {":scheme", "https"},
            {":path", "/images/logo.png"},
            {":authority", "cdn.example.com"},
            {"accept", "image/png,image/*"},
            {"user-agent", "TestClient/1.0"}
        }
    };
    
    for (const auto& headers : test_cases) {
        // 编码
        output_buffer buffer;
        auto encode_result = encoder->encode(headers, buffer);
        ASSERT_TRUE(encode_result.has_value());
        
        // 解码
        auto decode_result = decoder->decode(buffer.span());
        ASSERT_TRUE(decode_result.has_value());
        
        const auto& decoded_headers = decode_result.value();
        
        // 验证
        ASSERT_EQ(decoded_headers.size(), headers.size());
        for (size_t i = 0; i < headers.size(); ++i) {
            VerifyHeaderField(decoded_headers[i], headers[i].name, headers[i].value);
        }
    }
}

// =============================================================================
// HPACK 错误处理测试
// =============================================================================

TEST_F(HpackTest, DecodeInvalidIndex) {
    // 无效的表索引
    std::vector<uint8_t> encoded = {0xFF}; // 索引127，超出范围
    
    auto result = decoder->decode(std::span<const uint8_t>(encoded));
    EXPECT_FALSE(result.has_value());
}

TEST_F(HpackTest, DecodeIncompleteLiteral) {
    // 不完整的字面头部字段
    std::vector<uint8_t> encoded = {
        0x40, // 字面头部字段，增量索引
        0x05, // 名称长度：5
        'h', 'e', 'l', 'l' // 只有4个字符，少了1个
    };
    
    auto result = decoder->decode(std::span<const uint8_t>(encoded));
    EXPECT_FALSE(result.has_value());
}

TEST_F(HpackTest, DecodeOversizedString) {
    // 字符串长度声明过大
    std::vector<uint8_t> encoded = {
        0x40, // 字面头部字段，增量索引
        0x7F, 0x80, 0x01, // 名称长度：128 (使用整数编码)
        'a' // 只有1个字符
    };
    
    auto result = decoder->decode(std::span<const uint8_t>(encoded));
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// HPACK 性能测试
// =============================================================================

TEST_F(HpackTest, EncodeLargeHeaderSet) {
    // 创建大的头部集合
    std::vector<header_field> headers;
    headers.reserve(100);
    
    // 添加标准头部
    headers.emplace_back(":method", "GET");
    headers.emplace_back(":scheme", "https");
    headers.emplace_back(":path", "/api/large-response");
    headers.emplace_back(":authority", "api.example.com");
    
    // 添加大量自定义头部
    for (int i = 1; i <= 96; ++i) {
        headers.emplace_back("x-custom-header-" + std::to_string(i), 
                           "custom-value-" + std::to_string(i) + "-with-some-extra-data");
    }
    
    output_buffer buffer;
    auto result = encoder->encode(headers, buffer);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);
    
    // 解码验证
    auto decode_result = decoder->decode(buffer.span());
    ASSERT_TRUE(decode_result.has_value());
    
    const auto& decoded_headers = decode_result.value();
    EXPECT_EQ(decoded_headers.size(), headers.size());
}

TEST_F(HpackTest, EncodeRepeatedHeaders) {
    // 测试重复头部的压缩效果
    std::vector<header_field> headers1 = {
        {":method", "GET"},
        {":scheme", "https"},
        {":path", "/page1"},
        {":authority", "www.example.com"},
        {"user-agent", "Mozilla/5.0 (compatible; TestClient/1.0)"},
        {"accept", "text/html,application/xhtml+xml"},
        {"accept-language", "en-US,en;q=0.9"},
        {"cache-control", "no-cache"}
    };
    
    std::vector<header_field> headers2 = {
        {":method", "GET"},
        {":scheme", "https"},
        {":path", "/page2"}, // 只有path不同
        {":authority", "www.example.com"},
        {"user-agent", "Mozilla/5.0 (compatible; TestClient/1.0)"},
        {"accept", "text/html,application/xhtml+xml"},
        {"accept-language", "en-US,en;q=0.9"},
        {"cache-control", "no-cache"}
    };
    
    // 编码第一组头部
    output_buffer buffer1;
    auto result1 = encoder->encode(headers1, buffer1);
    ASSERT_TRUE(result1.has_value());
    
    // 编码第二组头部（应该更高效，因为大部分头部在动态表中）
    output_buffer buffer2;
    auto result2 = encoder->encode(headers2, buffer2);
    ASSERT_TRUE(result2.has_value());
    
    // 第二次编码应该更小（因为利用了动态表）
    EXPECT_LT(buffer2.size(), buffer1.size());
}

// =============================================================================
// HPACK Huffman编码测试
// =============================================================================

TEST_F(HpackTest, HuffmanEncodeDecode) {
    std::string test_string = "Mozilla/5.0 (compatible; bot)";
    
    // 测试Huffman编码
    output_buffer huffman_buffer;
    auto huffman_result = hpack_huffman::encode(test_string, huffman_buffer);
    EXPECT_TRUE(huffman_result.has_value());
    
    // 测试Huffman解码
    auto decoded_result = hpack_huffman::decode(huffman_buffer.span());
    ASSERT_TRUE(decoded_result.has_value());
    EXPECT_EQ(decoded_result.value(), test_string);
}

TEST_F(HpackTest, HuffmanCompressionRatio) {
    std::string long_string = "user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36";
    
    // Huffman编码
    output_buffer huffman_buffer;
    auto huffman_result = hpack_huffman::encode(long_string, huffman_buffer);
    ASSERT_TRUE(huffman_result.has_value());
    
    // 比较大小
    EXPECT_LT(huffman_buffer.size(), long_string.size());
    
    // 验证往返
    auto decoded = hpack_huffman::decode(huffman_buffer.span());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), long_string);
}

// =============================================================================
// HPACK 整数编码测试
// =============================================================================

TEST_F(HpackTest, IntegerEncoding) {
    output_buffer buffer;
    
    // 测试小整数
    auto result1 = hpack_integer::encode(5, 5, buffer);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(buffer.size(), 1);
    
    buffer.clear();
    
    // 测试大整数
    auto result2 = hpack_integer::encode(1337, 5, buffer);
    EXPECT_TRUE(result2.has_value());
    EXPECT_GT(buffer.size(), 1);
    
    // 解码验证
    size_t decoded_value;
    size_t consumed;
    auto decode_result = hpack_integer::decode(buffer.span(), 5, decoded_value, consumed);
    EXPECT_TRUE(decode_result.has_value());
    EXPECT_EQ(decoded_value, 1337);
    EXPECT_EQ(consumed, buffer.size());
}

TEST_F(HpackTest, IntegerBoundaryValues) {
    output_buffer buffer;
    
    // 测试边界值
    std::vector<size_t> test_values = {0, 1, 30, 31, 32, 255, 256, 1337, 65535, 65536};
    
    for (auto value : test_values) {
        buffer.clear();
        
        auto encode_result = hpack_integer::encode(value, 5, buffer);
        ASSERT_TRUE(encode_result.has_value()) << "Failed to encode " << value;
        
        size_t decoded_value;
        size_t consumed;
        auto decode_result = hpack_integer::decode(buffer.span(), 5, decoded_value, consumed);
        ASSERT_TRUE(decode_result.has_value()) << "Failed to decode " << value;
        EXPECT_EQ(decoded_value, value) << "Mismatch for value " << value;
        EXPECT_EQ(consumed, buffer.size());
    }
}