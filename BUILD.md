# HTTP Parse Library - Build Guide

This document provides comprehensive instructions for building the HTTP Parse library using CMake.

## Prerequisites

### Required
- **CMake 3.16+**
- **C++17 compatible compiler**:
  - GCC 7+
  - Clang 5+
  - MSVC 2017+

### Optional
- **C++20 compiler** for concepts and ranges support
- **C++23 compiler** for latest features (GCC 11+, Clang 12+)

## Quick Start

### Using the Build Script (Recommended)

The easiest way to build is using the provided build script:

```bash
# Basic build
./cmake_build.sh

# Debug build with examples and tests
./cmake_build.sh -t Debug -e -T

# C++23 build with all features
./cmake_build.sh -s 23 -e -T

# Clean build with 8 parallel jobs
./cmake_build.sh -c -j 8
```

### Manual CMake Commands

```bash
# Create build directory
mkdir build && cd build

# Configure (basic)
cmake ..

# Configure with options
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=20 \
      -DHTTP_PARSE_BUILD_EXAMPLES=ON \
      -DHTTP_PARSE_BUILD_TESTS=ON \
      ..

# Build
cmake --build . --parallel $(nproc)
```

### Using CMake Presets

```bash
# List available presets
cmake --list-presets

# Configure with preset
cmake --preset cxx23

# Build with preset
cmake --build --preset cxx23

# Test with preset
ctest --preset cxx23
```

## Build Options

### CMake Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | Build type: Debug, Release, RelWithDebInfo, MinSizeRel |
| `CMAKE_CXX_STANDARD` | `17` | C++ standard: 17, 20, 23 |
| `HTTP_PARSE_BUILD_EXAMPLES` | `ON` | Build example programs |
| `HTTP_PARSE_BUILD_TESTS` | `OFF` | Build test programs |
| `HTTP_PARSE_ENABLE_CXX23` | `ON` | Enable C++23 features if available |
| `HTTP_PARSE_INSTALL_EXAMPLES` | `OFF` | Install example binaries |

### Build Script Options

```bash
./cmake_build.sh --help
```

Options include:
- `-t, --type`: Build type (Debug, Release, etc.)
- `-s, --std`: C++ standard (17, 20, 23)
- `-e, --examples`: Build examples
- `-T, --tests`: Build tests
- `-c, --clean`: Clean build directory
- `-j, --jobs`: Parallel jobs
- `-p, --prefix`: Install prefix
- `-v, --verbose`: Verbose output

## Build Configurations

### Development Build
```bash
./cmake_build.sh -t Debug -s 23 -e -T -v
```
- Debug symbols
- C++23 features
- Examples and tests
- Verbose output

### Production Build
```bash
./cmake_build.sh -t Release -s 20
```
- Optimized for performance
- C++20 features
- Examples only

### Minimal Build
```bash
cmake --preset minimal
cmake --build --preset minimal
```
- Header-only library
- No examples or tests
- Fastest build

## Platform-Specific Instructions

### Linux/macOS

#### Using GCC
```bash
export CXX=g++
./cmake_build.sh -s 23
```

#### Using Clang
```bash
export CXX=clang++
./cmake_build.sh -s 23
```

### Windows

#### Visual Studio
```cmd
cmake -G "Visual Studio 16 2019" -A x64 ^
      -DCMAKE_CXX_STANDARD=20 ^
      -DHTTP_PARSE_BUILD_EXAMPLES=ON ^
      ..
cmake --build . --config Release
```

#### MinGW
```bash
cmake -G "MinGW Makefiles" \
      -DCMAKE_CXX_STANDARD=20 \
      ..
cmake --build .
```

## Feature Detection

The build system automatically detects available C++ features:

### C++20 Features
- Concepts
- Ranges
- Span (if not in C++17 mode)

### C++23 Features
- std::expected
- Enhanced ranges
- Latest concepts improvements

Feature availability is reported during configuration:
```
-- C++23 features available
-- Enabled C++23 standard
```

## Running Examples

After building with examples enabled:

```bash
# Run all examples
make run_examples

# Run specific examples
./examples/basic_http1_parsing
./examples/http2_connection
./examples/version_detection
./examples/incremental_parsing

# C++23 example (if built)
./examples/cpp23_features
```

## Running Tests

After building with tests enabled:

```bash
# Using make
make run_tests

# Using CTest
ctest --output-on-failure

# Specific test
./tests/test_basic_parsing
```

## Installation

### System Installation
```bash
# Configure with install prefix
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..

# Build and install
cmake --build .
sudo cmake --install .
```

### Local Installation
```bash
# Install to custom directory
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..
cmake --build .
cmake --install .
```

### Using the Library

After installation, use in your CMake project:

```cmake
find_package(http_parse REQUIRED)
target_link_libraries(your_target PRIVATE co::http_parse)

# Enable C++23 features (optional)
target_enable_http_parse_cxx23(your_target)
```

## IDE Integration

### Visual Studio Code
1. Install CMake Tools extension
2. Open project folder
3. Select preset: `Ctrl+Shift+P` → "CMake: Select Configure Preset"
4. Build: `Ctrl+Shift+P` → "CMake: Build"

### CLion
1. Open project folder
2. CLion automatically detects CMakeLists.txt
3. Choose configuration in CMake settings
4. Build using IDE controls

### Visual Studio
1. Open folder
2. Visual Studio detects CMake project
3. Select configuration
4. Build → Build All

## Troubleshooting

### Common Issues

#### C++23 Features Not Available
```
-- C++20 features available
-- Using C++17 compatibility mode
```
**Solution**: Update compiler or use `-s 20` for C++20 features

#### Missing Examples
```
-- Skipping cpp23_features - requires C++23 features
```
**Solution**: Build with C++23 compiler or disable C++23 example

#### Build Failures
```
error: 'std::expected' has not been declared
```
**Solution**: Use C++17/20 mode or update compiler

### Debug Build Issues

Enable verbose output to diagnose problems:
```bash
./cmake_build.sh -v -t Debug
```

### Compiler Compatibility

| Compiler | C++17 | C++20 | C++23 |
|----------|-------|-------|-------|
| GCC 7    | ✅    | ❌    | ❌    |
| GCC 10   | ✅    | ✅    | ⚠️    |
| GCC 11+  | ✅    | ✅    | ✅    |
| Clang 5  | ✅    | ❌    | ❌    |
| Clang 10 | ✅    | ✅    | ⚠️    |
| Clang 12+| ✅    | ✅    | ✅    |
| MSVC 2017| ✅    | ❌    | ❌    |
| MSVC 2019| ✅    | ✅    | ⚠️    |
| MSVC 2022| ✅    | ✅    | ✅    |

Legend: ✅ Full support, ⚠️ Partial support, ❌ No support

## Performance Optimization

### Release Builds
Always use Release builds for performance testing:
```bash
./cmake_build.sh -t Release
```

### Link Time Optimization (LTO)
```bash
cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..
```

### Native Optimization
```bash
cmake -DCMAKE_CXX_FLAGS="-march=native" ..
```

## Custom Integration

### As Subdirectory
```cmake
add_subdirectory(third_party/http_parse)
target_link_libraries(your_target PRIVATE co::http_parse)
```

### With FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
    http_parse
    GIT_REPOSITORY https://github.com/your-org/http_parse.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(http_parse)
target_link_libraries(your_target PRIVATE co::http_parse)
```

## Contributing

When contributing to the build system:

1. Test all presets: `cmake --list-presets`
2. Verify cross-platform compatibility
3. Update this documentation
4. Test with minimum required CMake version

## Support

For build-related issues:
1. Check compiler compatibility table
2. Try different presets
3. Enable verbose output
4. Consult troubleshooting section
5. Open issue with build logs