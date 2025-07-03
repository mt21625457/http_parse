cmake_minimum_required(VERSION 3.20)
project(http_parse_tests)

# 设置C++23标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")

# 包含目录
include_directories(../include)

# 查找GoogleTest
find_package(GTest REQUIRED)
if(NOT GTest_FOUND)
    # 如果没有找到，使用FetchContent下载
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50c5c7fa2b6a85e0e6b22b7d6b7c2c.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

# 查找线程库
find_package(Threads REQUIRED)

# 测试可执行文件
set(TEST_SOURCES
    test_main.cpp
    test_http1_parser.cpp
    test_http1_encoder.cpp
    test_http1_builder.cpp
    test_http2_types.cpp
    test_http2_stream_manager.cpp
    test_http2_frame_processor.cpp
    test_hpack.cpp
    test_buffer.cpp
    test_unified_api.cpp
    benchmark_tests.cpp
    stress_tests.cpp
)

add_executable(http_parse_tests ${TEST_SOURCES})

# 链接库
target_link_libraries(http_parse_tests 
    GTest::gtest 
    GTest::gtest_main
    Threads::Threads
)

# 启用CTest
enable_testing()

# 添加测试
add_test(NAME http_parse_unit_tests COMMAND http_parse_tests)

# 添加自定义目标
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS http_parse_tests
)

add_custom_target(run_benchmarks
    COMMAND ./http_parse_tests --gtest_filter="*Benchmark*"
    DEPENDS http_parse_tests
)

add_custom_target(run_stress_tests
    COMMAND ./http_parse_tests --gtest_filter="*Stress*"
    DEPENDS http_parse_tests
)