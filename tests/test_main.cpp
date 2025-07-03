#include <gtest/gtest.h>
#include <iostream>
#include <chrono>

// 全局测试环境
class HttpParseTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        std::cout << "\n=== HTTP Parse Library 单元测试开始 ===\n";
        std::cout << "测试时间: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
        std::cout << "编译器: " << __VERSION__ << "\n";
        std::cout << "C++标准: " << __cplusplus << "\n\n";
    }
    
    void TearDown() override {
        std::cout << "\n=== HTTP Parse Library 单元测试完成 ===\n";
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // 添加全局测试环境
    ::testing::AddGlobalTestEnvironment(new HttpParseTestEnvironment);
    
    // 设置测试输出格式
    ::testing::FLAGS_gtest_color = "yes";
    ::testing::FLAGS_gtest_print_time = true;
    
    return RUN_ALL_TESTS();
}