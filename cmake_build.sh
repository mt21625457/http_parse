#!/bin/bash

# HTTP Parse Library Build Script
# This script provides convenient build options for the HTTP Parse library

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Default values
BUILD_TYPE="Release"
CXX_STANDARD="17"
BUILD_EXAMPLES="ON"
BUILD_TESTS="OFF"
ENABLE_CXX23="ON"
BUILD_DIR="build"
INSTALL_PREFIX=""
VERBOSE="OFF"

# Help function
show_help() {
    cat << EOF
HTTP Parse Library Build Script

Usage: $0 [OPTIONS]

Options:
    -h, --help              Show this help message
    -t, --type TYPE         Build type: Debug, Release, RelWithDebInfo, MinSizeRel (default: Release)
    -s, --std STANDARD      C++ standard: 17, 20, 23 (default: 17)
    -e, --examples          Build examples (default: ON)
    -T, --tests             Build tests (default: OFF)
    --no-cxx23              Disable C++23 features
    -d, --dir DIRECTORY     Build directory (default: build)
    -p, --prefix PATH       Install prefix
    -v, --verbose           Verbose build output
    -c, --clean             Clean build directory before building
    -j, --jobs N            Number of parallel jobs (default: auto-detect)

Examples:
    $0                                    # Basic build with defaults
    $0 -t Debug -e -T                    # Debug build with examples and tests
    $0 -s 23 --examples                  # C++23 build with examples
    $0 -c -j 8                           # Clean build with 8 parallel jobs
    $0 -p /usr/local                     # Build and specify install prefix

Build Types:
    Debug          - Debug information, no optimization
    Release        - Optimized for speed
    RelWithDebInfo - Optimized with debug info
    MinSizeRel     - Optimized for size

C++ Standards:
    17             - C++17 (minimum required)
    20             - C++20 (enables concepts, ranges)
    23             - C++23 (enables expected, latest features)
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -s|--std)
            CXX_STANDARD="$2"
            shift 2
            ;;
        -e|--examples)
            BUILD_EXAMPLES="ON"
            shift
            ;;
        -T|--tests)
            BUILD_TESTS="ON"
            shift
            ;;
        --no-cxx23)
            ENABLE_CXX23="OFF"
            shift
            ;;
        -d|--dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="ON"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD="ON"
            shift
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate build type
case $BUILD_TYPE in
    Debug|Release|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        print_error "Invalid build type: $BUILD_TYPE"
        exit 1
        ;;
esac

# Validate C++ standard
case $CXX_STANDARD in
    17|20|23)
        ;;
    *)
        print_error "Invalid C++ standard: $CXX_STANDARD"
        exit 1
        ;;
esac

# Auto-detect number of CPU cores if not specified
if [[ -z "$PARALLEL_JOBS" ]]; then
    if command -v nproc >/dev/null 2>&1; then
        PARALLEL_JOBS=$(nproc)
    elif command -v sysctl >/dev/null 2>&1; then
        PARALLEL_JOBS=$(sysctl -n hw.ncpu)
    else
        PARALLEL_JOBS=4
    fi
fi

print_status "HTTP Parse Library Build Configuration"
echo "======================================"
echo "Build Type:       $BUILD_TYPE"
echo "C++ Standard:     C++$CXX_STANDARD"
echo "Build Examples:   $BUILD_EXAMPLES"
echo "Build Tests:      $BUILD_TESTS"
echo "Enable C++23:     $ENABLE_CXX23"
echo "Build Directory:  $BUILD_DIR"
echo "Parallel Jobs:    $PARALLEL_JOBS"
if [[ -n "$INSTALL_PREFIX" ]]; then
    echo "Install Prefix:   $INSTALL_PREFIX"
fi
echo "======================================"

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == "ON" ]] && [[ -d "$BUILD_DIR" ]]; then
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Prepare CMake arguments
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_CXX_STANDARD="$CXX_STANDARD"
    -DHTTP_PARSE_BUILD_EXAMPLES="$BUILD_EXAMPLES"
    -DHTTP_PARSE_BUILD_TESTS="$BUILD_TESTS"
    -DHTTP_PARSE_ENABLE_CXX23="$ENABLE_CXX23"
)

if [[ -n "$INSTALL_PREFIX" ]]; then
    CMAKE_ARGS+=(-DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX")
fi

if [[ "$VERBOSE" == "ON" ]]; then
    CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
fi

# Configure
print_status "Configuring with CMake..."
cmake "${CMAKE_ARGS[@]}" ..

if [[ $? -ne 0 ]]; then
    print_error "CMake configuration failed"
    exit 1
fi

# Build
print_status "Building..."
if [[ "$VERBOSE" == "ON" ]]; then
    cmake --build . --parallel "$PARALLEL_JOBS" --verbose
else
    cmake --build . --parallel "$PARALLEL_JOBS"
fi

if [[ $? -ne 0 ]]; then
    print_error "Build failed"
    exit 1
fi

print_success "Build completed successfully!"

# Show what was built
echo ""
print_status "Build Results:"
if [[ "$BUILD_EXAMPLES" == "ON" ]] && [[ -d "examples" ]]; then
    echo "Examples built in: $BUILD_DIR/examples/"
    ls -la examples/ | grep -E "^-.*x.*" | awk '{print "  " $9}'
fi

if [[ "$BUILD_TESTS" == "ON" ]] && [[ -d "tests" ]]; then
    echo "Tests built in: $BUILD_DIR/tests/"
    ls -la tests/ | grep -E "^-.*x.*" | awk '{print "  " $9}'
fi

# Offer to run examples
if [[ "$BUILD_EXAMPLES" == "ON" ]]; then
    echo ""
    print_status "To run examples:"
    echo "  make run_examples           # Run all examples"
    echo "  ./examples/basic_http1_parsing   # Run specific example"
fi

# Offer to run tests
if [[ "$BUILD_TESTS" == "ON" ]]; then
    echo ""
    print_status "To run tests:"
    echo "  make run_tests              # Run all tests"
    echo "  ctest --output-on-failure   # Run with CTest"
fi

# Installation instructions
if [[ -n "$INSTALL_PREFIX" ]]; then
    echo ""
    print_status "To install:"
    echo "  cmake --install ."
fi

print_success "HTTP Parse library is ready to use!"