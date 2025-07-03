cmake_minimum_required(VERSION 3.16)
project(http_parse 
    VERSION 1.0.0
    DESCRIPTION "Modern C++ HTTP/1.x and HTTP/2 protocol parsing library"
    LANGUAGES CXX
)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# C++ standard requirements - Force C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
    if(CMAKE_CXX_STANDARD GREATER_EQUAL 23)
        add_compile_options(/std:c++latest)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    
    # Enable C++20/23 features if available
    if(CMAKE_CXX_STANDARD GREATER_EQUAL 23)
        add_compile_options(-std=c++23)
    elseif(CMAKE_CXX_STANDARD GREATER_EQUAL 20)
        add_compile_options(-std=c++20)
    endif()
endif()

# Header-only library interface
add_library(http_parse INTERFACE)
add_library(co::http_parse ALIAS http_parse)

target_include_directories(http_parse INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(http_parse INTERFACE cxx_std_23)

# Feature detection
include(CheckCXXSourceCompiles)

# Check for C++20 features
set(CMAKE_REQUIRED_FLAGS "-std=c++20")
check_cxx_source_compiles("
    #include <concepts>
    #include <span>
    template<typename T>
    concept integral = std::is_integral_v<T>;
    int main() { return 0; }
" HAS_CXX20_FEATURES)

# Check for C++23 features
set(CMAKE_REQUIRED_FLAGS "-std=c++23")
check_cxx_source_compiles("
    #include <expected>
    #include <ranges>
    int main() { 
        std::expected<int, int> e{42};
        return 0; 
    }
" HAS_CXX23_FEATURES)

# Configure compile definitions
if(HAS_CXX23_FEATURES)
    target_compile_definitions(http_parse INTERFACE HTTP_PARSE_HAS_CXX23=1)
    message(STATUS "C++23 features available")
elseif(HAS_CXX20_FEATURES)
    target_compile_definitions(http_parse INTERFACE HTTP_PARSE_HAS_CXX20=1)
    message(STATUS "C++20 features available")
else()
    message(STATUS "Using C++17 compatibility mode")
endif()

# Option to build examples
option(HTTP_PARSE_BUILD_EXAMPLES "Build example programs" ON)

# Option to build tests
option(HTTP_PARSE_BUILD_TESTS "Build test programs" OFF)

# Option to enable C++23 features
option(HTTP_PARSE_ENABLE_CXX23 "Enable C++23 features if available" ON)

if(HTTP_PARSE_ENABLE_CXX23 AND HAS_CXX23_FEATURES)
    set(CMAKE_CXX_STANDARD 23)
    target_compile_features(http_parse INTERFACE cxx_std_23)
    message(STATUS "Enabled C++23 standard")
elseif(HAS_CXX20_FEATURES)
    set(CMAKE_CXX_STANDARD 20)
    target_compile_features(http_parse INTERFACE cxx_std_20)
    message(STATUS "Enabled C++20 standard")
endif()

# Examples
if(HTTP_PARSE_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# Tests
if(HTTP_PARSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS http_parse
    EXPORT http_parse-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT http_parse-targets
    FILE http_parse-targets.cmake
    NAMESPACE co::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/http_parse
)

# Generate and install config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/http_parse-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/http_parse-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/http_parse
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/http_parse-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/http_parse-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/http_parse-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/http_parse
)

# Export for build tree
export(EXPORT http_parse-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/http_parse-targets.cmake"
    NAMESPACE co::
)

# Package information
set(CPACK_PACKAGE_NAME "http_parse")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_CONTACT "your-email@example.com")

include(CPack)