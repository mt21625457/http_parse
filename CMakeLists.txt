cmake_minimum_required(VERSION 3.16)
project(http_parse 
    VERSION 1.0.0
    DESCRIPTION "Modern C++ HTTP/1.x and HTTP/2 protocol parsing library"
    LANGUAGES CXX
)


# C++ standard requirements - Force C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable LTO to avoid linker issues
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# Include directories
if(LIBBASE_BASE)
    include_directories(${LIBBASE_BASE}/include)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${LIBBASE_BASE}/lib/cmake)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create separate executables for each example
add_executable(comprehensive_example example/comprehensive_example.cpp)
add_executable(http1_example example/http1_example.cpp)
add_executable(http2_example example/http2_example.cpp)




find_package(GTest REQUIRED)

set(tests_src
    tests/benchmark_tests.cpp
    tests/stress_tests.cpp
    tests/test_buffer.cpp
    tests/test_hpack.cpp
    tests/test_http1_builder.cpp
    tests/test_http1_encoder.cpp
    tests/test_http1_parser.cpp
    tests/test_http2_frame_processor.cpp
    tests/test_http2_stream_manager.cpp
    tests/test_http2_types.cpp
    tests/test_main.cpp
    tests/test_unified_api.cpp
)


add_executable(http_parse_tests ${tests_src})


target_link_libraries(http_parse_tests PUBLIC  GTest::GTest GTest::Main)








