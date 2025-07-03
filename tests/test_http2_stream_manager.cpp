#include <gtest/gtest.h>
#include "http_parse.hpp"
#include "http_parse/v2/stream_manager.hpp"
#include <string>
#include <vector>

using namespace co::http::v2;

class Http2StreamManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
        manager = std::make_unique<stream_manager>();
    }
    
    void TearDown() override {
        // 每个测试后的清理
        manager.reset();
    }
    
    std::unique_ptr<stream_manager> manager;
};

// =============================================================================
// HTTP/2 流管理器基本功能测试
// =============================================================================

TEST_F(Http2StreamManagerTest, CreateClientStream) {
    // 客户端流ID必须是奇数
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    EXPECT_EQ(stream.value()->id, 1);
    EXPECT_EQ(stream.value()->state, stream_state::idle);
    EXPECT_FALSE(stream.value()->is_server_stream);
}

TEST_F(Http2StreamManagerTest, CreateServerStream) {
    // 服务器流ID必须是偶数
    auto stream = manager->create_stream(2, true);
    ASSERT_TRUE(stream.has_value());
    EXPECT_EQ(stream.value()->id, 2);
    EXPECT_EQ(stream.value()->state, stream_state::idle);
    EXPECT_TRUE(stream.value()->is_server_stream);
}

TEST_F(Http2StreamManagerTest, StreamIdValidation) {
    // 流ID为0是无效的
    auto invalid_stream = manager->create_stream(0, false);
    EXPECT_FALSE(invalid_stream.has_value());
    
    // 客户端不能创建偶数流ID
    auto invalid_client = manager->create_stream(2, false);
    EXPECT_FALSE(invalid_client.has_value());
    
    // 服务器不能创建奇数流ID
    auto invalid_server = manager->create_stream(1, true);
    EXPECT_FALSE(invalid_server.has_value());
}

TEST_F(Http2StreamManagerTest, DuplicateStreamId) {
    // 创建第一个流
    auto stream1 = manager->create_stream(1, false);
    ASSERT_TRUE(stream1.has_value());
    
    // 尝试创建相同ID的流应该失败
    auto duplicate = manager->create_stream(1, false);
    EXPECT_FALSE(duplicate.has_value());
}

TEST_F(Http2StreamManagerTest, FindStream) {
    // 创建几个流
    auto stream1 = manager->create_stream(1, false);
    auto stream3 = manager->create_stream(3, false);
    auto stream5 = manager->create_stream(5, false);
    
    ASSERT_TRUE(stream1.has_value());
    ASSERT_TRUE(stream3.has_value());
    ASSERT_TRUE(stream5.has_value());
    
    // 查找存在的流
    auto found1 = manager->find_stream(1);
    auto found3 = manager->find_stream(3);
    auto found5 = manager->find_stream(5);
    
    ASSERT_TRUE(found1.has_value());
    ASSERT_TRUE(found3.has_value());
    ASSERT_TRUE(found5.has_value());
    
    EXPECT_EQ(found1.value()->id, 1);
    EXPECT_EQ(found3.value()->id, 3);
    EXPECT_EQ(found5.value()->id, 5);
    
    // 查找不存在的流
    auto not_found = manager->find_stream(7);
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(Http2StreamManagerTest, RemoveStream) {
    // 创建流
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    
    // 确认流存在
    auto found = manager->find_stream(1);
    ASSERT_TRUE(found.has_value());
    
    // 删除流
    auto removed = manager->remove_stream(1);
    EXPECT_TRUE(removed.has_value());
    
    // 确认流不存在
    auto not_found = manager->find_stream(1);
    EXPECT_FALSE(not_found.has_value());
    
    // 删除不存在的流
    auto remove_again = manager->remove_stream(1);
    EXPECT_FALSE(remove_again.has_value());
}

// =============================================================================
// HTTP/2 流状态管理测试
// =============================================================================

TEST_F(Http2StreamManagerTest, StreamStateTransitions) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    
    auto* s = stream.value();
    EXPECT_EQ(s->state, stream_state::idle);
    
    // idle -> open
    auto result = manager->update_stream_state(1, stream_state::open);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->state, stream_state::open);
    
    // open -> half_closed_local
    result = manager->update_stream_state(1, stream_state::half_closed_local);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->state, stream_state::half_closed_local);
    
    // half_closed_local -> closed
    result = manager->update_stream_state(1, stream_state::closed);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->state, stream_state::closed);
}

TEST_F(Http2StreamManagerTest, StreamStateQueries) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    auto* s = stream.value();
    
    // 初始状态
    EXPECT_FALSE(s->is_closed());
    EXPECT_FALSE(s->is_half_closed_local());
    EXPECT_FALSE(s->is_half_closed_remote());
    EXPECT_TRUE(s->can_send_data());
    EXPECT_TRUE(s->can_receive_data());
    
    // 设置为半关闭状态
    manager->update_stream_state(1, stream_state::half_closed_local);
    EXPECT_FALSE(s->is_closed());
    EXPECT_TRUE(s->is_half_closed_local());
    EXPECT_FALSE(s->is_half_closed_remote());
    EXPECT_FALSE(s->can_send_data());
    EXPECT_TRUE(s->can_receive_data());
    
    // 设置为关闭状态
    manager->update_stream_state(1, stream_state::closed);
    EXPECT_TRUE(s->is_closed());
    EXPECT_FALSE(s->can_send_data());
    EXPECT_FALSE(s->can_receive_data());
}

// =============================================================================
// HTTP/2 流量控制测试
// =============================================================================

TEST_F(Http2StreamManagerTest, StreamWindowUpdate) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    auto* s = stream.value();
    
    // 初始窗口大小
    EXPECT_EQ(s->window_size, 65535);
    EXPECT_EQ(s->remote_window_size, 65535);
    
    // 更新本地窗口
    auto result = manager->update_stream_window(1, 1000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->window_size, 66535);
    
    // 减少本地窗口
    result = manager->update_stream_window(1, -2000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->window_size, 64535);
    
    // 更新远程窗口
    result = manager->update_remote_window(1, 5000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->remote_window_size, 70535);
}

TEST_F(Http2StreamManagerTest, WindowOverflow) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    auto* s = stream.value();
    
    // 尝试增加到超过最大值
    auto result = manager->update_stream_window(1, INT32_MAX);
    EXPECT_FALSE(result.has_value()); // 应该失败
    
    // 尝试减少到负数
    result = manager->update_stream_window(1, -100000);
    EXPECT_FALSE(result.has_value()); // 应该失败
}

TEST_F(Http2StreamManagerTest, WindowFlowControl) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    auto* s = stream.value();
    
    // 窗口为0时不能发送数据
    manager->update_stream_window(1, -s->window_size);
    EXPECT_EQ(s->window_size, 0);
    EXPECT_FALSE(s->can_send_data());
    
    // 恢复窗口
    manager->update_stream_window(1, 1000);
    EXPECT_GT(s->window_size, 0);
    EXPECT_TRUE(s->can_send_data());
}

// =============================================================================
// HTTP/2 流优先级测试
// =============================================================================

TEST_F(Http2StreamManagerTest, StreamPriority) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    auto* s = stream.value();
    
    // 默认优先级
    EXPECT_EQ(s->weight, 16);
    EXPECT_EQ(s->dependency, 0);
    EXPECT_FALSE(s->exclusive);
    
    // 设置优先级
    auto result = manager->update_stream_priority(1, 31, 200, true);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(s->dependency, 31);
    EXPECT_EQ(s->weight, 200);
    EXPECT_TRUE(s->exclusive);
}

TEST_F(Http2StreamManagerTest, PriorityDependency) {
    // 创建依赖链：1 -> 3 -> 5
    auto stream1 = manager->create_stream(1, false);
    auto stream3 = manager->create_stream(3, false);
    auto stream5 = manager->create_stream(5, false);
    
    ASSERT_TRUE(stream1.has_value());
    ASSERT_TRUE(stream3.has_value());
    ASSERT_TRUE(stream5.has_value());
    
    // 设置依赖关系
    manager->update_stream_priority(3, 1, 100, false);
    manager->update_stream_priority(5, 3, 50, false);
    
    auto* s1 = stream1.value();
    auto* s3 = stream3.value();
    auto* s5 = stream5.value();
    
    EXPECT_EQ(s3->dependency, 1);
    EXPECT_EQ(s5->dependency, 3);
    
    // 检查依赖链
    auto deps = manager->get_stream_dependencies(1);
    EXPECT_EQ(deps.size(), 1);
    EXPECT_EQ(deps[0], 3);
    
    deps = manager->get_stream_dependencies(3);
    EXPECT_EQ(deps.size(), 1);
    EXPECT_EQ(deps[0], 5);
}

TEST_F(Http2StreamManagerTest, CircularDependency) {
    auto stream1 = manager->create_stream(1, false);
    auto stream3 = manager->create_stream(3, false);
    
    ASSERT_TRUE(stream1.has_value());
    ASSERT_TRUE(stream3.has_value());
    
    // 设置：1 -> 3
    manager->update_stream_priority(3, 1, 100, false);
    
    // 尝试设置：3 -> 1 (形成循环)
    auto result = manager->update_stream_priority(1, 3, 100, false);
    EXPECT_FALSE(result.has_value()); // 应该失败
}

// =============================================================================
// HTTP/2 连接级别管理测试
// =============================================================================

TEST_F(Http2StreamManagerTest, ConnectionWindow) {
    // 测试连接级别窗口
    EXPECT_EQ(manager->get_connection_window(), 65535);
    EXPECT_EQ(manager->get_remote_connection_window(), 65535);
    
    // 更新连接窗口
    auto result = manager->update_connection_window(1000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager->get_connection_window(), 66535);
    
    // 更新远程连接窗口
    result = manager->update_remote_connection_window(-2000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(manager->get_remote_connection_window(), 63535);
}

TEST_F(Http2StreamManagerTest, MaxConcurrentStreams) {
    // 设置最大并发流数量
    manager->set_max_concurrent_streams(2);
    
    // 创建两个流
    auto stream1 = manager->create_stream(1, false);
    auto stream3 = manager->create_stream(3, false);
    
    ASSERT_TRUE(stream1.has_value());
    ASSERT_TRUE(stream3.has_value());
    
    // 尝试创建第三个流应该失败
    auto stream5 = manager->create_stream(5, false);
    EXPECT_FALSE(stream5.has_value());
    
    // 关闭一个流后应该可以创建新流
    manager->update_stream_state(1, stream_state::closed);
    stream5 = manager->create_stream(5, false);
    EXPECT_TRUE(stream5.has_value());
}

TEST_F(Http2StreamManagerTest, StreamCount) {
    EXPECT_EQ(manager->get_stream_count(), 0);
    
    // 创建几个流
    manager->create_stream(1, false);
    manager->create_stream(3, false);
    manager->create_stream(5, false);
    
    EXPECT_EQ(manager->get_stream_count(), 3);
    
    // 删除一个流
    manager->remove_stream(3);
    EXPECT_EQ(manager->get_stream_count(), 2);
}

// =============================================================================
// HTTP/2 错误处理测试
// =============================================================================

TEST_F(Http2StreamManagerTest, StreamErrorHandling) {
    auto stream = manager->create_stream(1, false);
    ASSERT_TRUE(stream.has_value());
    auto* s = stream.value();
    
    // 设置流错误
    manager->set_stream_error(1, h2_error_code::internal_error);
    EXPECT_EQ(s->error, h2_error_code::internal_error);
    
    // 有错误的流状态查询
    EXPECT_TRUE(s->has_error());
    EXPECT_FALSE(s->can_send_data());
    EXPECT_FALSE(s->can_receive_data());
}

TEST_F(Http2StreamManagerTest, InvalidStreamOperations) {
    // 对不存在的流进行操作
    auto result = manager->update_stream_state(999, stream_state::open);
    EXPECT_FALSE(result.has_value());
    
    result = manager->update_stream_window(999, 1000);
    EXPECT_FALSE(result.has_value());
    
    result = manager->update_stream_priority(999, 1, 100, false);
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// HTTP/2 性能和压力测试
// =============================================================================

TEST_F(Http2StreamManagerTest, ManyStreams) {
    // 创建大量流
    const int num_streams = 1000;
    
    for (int i = 1; i <= num_streams; i += 2) {
        auto stream = manager->create_stream(i, false);
        ASSERT_TRUE(stream.has_value()) << "Failed to create stream " << i;
    }
    
    EXPECT_EQ(manager->get_stream_count(), num_streams / 2);
    
    // 查找随机流
    auto found = manager->find_stream(501);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found.value()->id, 501);
    
    // 删除一半流
    for (int i = 1; i <= num_streams / 2; i += 2) {
        auto removed = manager->remove_stream(i);
        EXPECT_TRUE(removed.has_value());
    }
    
    EXPECT_EQ(manager->get_stream_count(), num_streams / 4);
}

TEST_F(Http2StreamManagerTest, StreamIterator) {
    // 创建多个流
    std::vector<uint32_t> stream_ids = {1, 3, 5, 7, 9};
    
    for (auto id : stream_ids) {
        auto stream = manager->create_stream(id, false);
        ASSERT_TRUE(stream.has_value());
    }
    
    // 遍历所有流
    auto all_streams = manager->get_all_streams();
    EXPECT_EQ(all_streams.size(), stream_ids.size());
    
    // 验证流ID
    std::vector<uint32_t> found_ids;
    for (const auto& stream : all_streams) {
        found_ids.push_back(stream->id);
    }
    
    std::sort(found_ids.begin(), found_ids.end());
    std::sort(stream_ids.begin(), stream_ids.end());
    EXPECT_EQ(found_ids, stream_ids);
}

TEST_F(Http2StreamManagerTest, StreamCleanup) {
    // 创建多个流
    for (int i = 1; i <= 10; i += 2) {
        auto stream = manager->create_stream(i, false);
        ASSERT_TRUE(stream.has_value());
        
        // 模拟流完成
        manager->update_stream_state(i, stream_state::closed);
    }
    
    // 清理已关闭的流
    auto cleaned = manager->cleanup_closed_streams();
    EXPECT_EQ(cleaned, 5); // 应该清理了5个流
    EXPECT_EQ(manager->get_stream_count(), 0);
}

// =============================================================================
// HTTP/2 重置和恢复测试
// =============================================================================

TEST_F(Http2StreamManagerTest, ManagerReset) {
    // 创建一些流
    manager->create_stream(1, false);
    manager->create_stream(3, false);
    manager->create_stream(5, false);
    
    EXPECT_EQ(manager->get_stream_count(), 3);
    
    // 重置管理器
    manager->reset();
    
    EXPECT_EQ(manager->get_stream_count(), 0);
    EXPECT_EQ(manager->get_connection_window(), 65535);
    EXPECT_EQ(manager->get_remote_connection_window(), 65535);
    
    // 重置后应该能正常创建新流
    auto stream = manager->create_stream(1, false);
    EXPECT_TRUE(stream.has_value());
}

TEST_F(Http2StreamManagerTest, StreamStatistics) {
    // 创建不同状态的流
    auto stream1 = manager->create_stream(1, false);
    auto stream3 = manager->create_stream(3, false);
    auto stream5 = manager->create_stream(5, false);
    
    ASSERT_TRUE(stream1.has_value());
    ASSERT_TRUE(stream3.has_value());
    ASSERT_TRUE(stream5.has_value());
    
    // 设置不同状态
    manager->update_stream_state(1, stream_state::open);
    manager->update_stream_state(3, stream_state::half_closed_local);
    manager->update_stream_state(5, stream_state::closed);
    
    // 获取统计信息
    auto stats = manager->get_stream_statistics();
    EXPECT_EQ(stats.total_streams, 3);
    EXPECT_EQ(stats.idle_streams, 0);
    EXPECT_EQ(stats.open_streams, 1);
    EXPECT_EQ(stats.half_closed_streams, 1);
    EXPECT_EQ(stats.closed_streams, 1);
}