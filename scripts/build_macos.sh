#!/bin/bash

# macOS build script for QtMultiThreadNetwork

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN_BUILD=false
QT_VERSION=""
VERBOSE=false
INSTALL_PREFIX=""
RUN_TESTS=false
ARCH=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug) BUILD_TYPE="Debug"; shift ;;
        --release) BUILD_TYPE="Release"; shift ;;
        --relwithdebinfo) BUILD_TYPE="RelWithDebInfo"; shift ;;
        --minsizerel) BUILD_TYPE="MinSizeRel"; shift ;;
        --clean) CLEAN_BUILD=true; shift ;;
        --qt-version) QT_VERSION="$2"; shift 2 ;;
        --prefix) INSTALL_PREFIX="-DCMAKE_INSTALL_PREFIX=$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --tests) RUN_TESTS=true; shift ;;
        --arch) ARCH="$2"; shift 2 ;;
        -v|--verbose) VERBOSE=true; shift ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --debug             Build in Debug mode"
            echo "  --release           Build in Release mode (default)"
            echo "  --relwithdebinfo    Build in Release with Debug Info mode"
            echo "  --minsizerel        Build in Minimum Size Release mode"
            echo "  --clean             Clean build directory before building"
            echo "  --qt-version VER    Specify Qt version (e.g., 5, 6)"
            echo "  --prefix DIR        Install prefix directory"
            echo "  --build-dir DIR     Build directory name (default: build)"
            echo "  --arch ARCH         Set CMAKE_OSX_ARCHITECTURES (e.g., x86_64, arm64)"
            echo "  --tests             Run tests after building"
            echo "  -v, --verbose       Verbose output"
            echo "  -h, --help          Show this help message"
            exit 0
            ;;
        *) print_error "Unknown option: $1"; exit 1 ;;
    esac
done

print_status "Starting macOS build for QtMultiThreadNetwork"
print_status "Build type: $BUILD_TYPE"
print_status "Build directory: $BUILD_DIR"

if [[ "$(basename "$(pwd)")" != "scripts" ]]; then
    print_error "Please run this script from the 'scripts' directory"
    exit 1
fi

cd ..

# Homebrew OpenSSL
if command -v brew &> /dev/null; then
    OPENSSL_PREFIX=$(brew --prefix openssl@1.1 2>/dev/null || brew --prefix openssl@3 2>/dev/null || true)
    if [[ -n "$OPENSSL_PREFIX" ]]; then
        export OPENSSL_ROOT_DIR="$OPENSSL_PREFIX"
        print_status "Found OpenSSL at: $OPENSSL_ROOT_DIR"
    else
        print_warning "OpenSSL not found via Homebrew. HTTPS requests may fail."
        print_warning "Install with: brew install openssl@1.1"
    fi
elif [[ -n "$OPENSSL_ROOT_DIR" ]]; then
    print_status "Using OPENSSL_ROOT_DIR from environment: $OPENSSL_ROOT_DIR"
else
    print_warning "OPENSSL_ROOT_DIR not set. HTTPS requests may fail."
fi

# Qt auto-detect
if [[ -z "$QT_VERSION" ]]; then
    if command -v qmake6 &> /dev/null; then
        QT_VERSION="6"
        print_status "Detected Qt6"
    elif command -v qmake &> /dev/null; then
        QT_VERSION="5"
        print_status "Detected Qt5"
    else
        print_error "Qt not found. Install via Homebrew: brew install qt@5"
        exit 1
    fi
fi

# Required tools
print_status "Checking system dependencies..."
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Install: brew install cmake"
    exit 1
fi
if ! command -v clang++ &> /dev/null; then
    print_error "Clang not found. Install Xcode: xcode-select --install"
    exit 1
fi
if ! command -v make &> /dev/null; then
    print_error "Make not found. Install Xcode command line tools."
    exit 1
fi

# Clean
if [[ "$CLEAN_BUILD" == true ]]; then
    print_status "Cleaning build directory..."
    [[ -d "$BUILD_DIR" ]] && rm -rf "$BUILD_DIR"
fi

[[ ! -d "$BUILD_DIR" ]] && mkdir -p "$BUILD_DIR"

# Configure
print_status "Configuring project with CMake..."
CMAKE_ARGS=(
    -S .
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    $INSTALL_PREFIX
)

[[ "$VERBOSE" == true ]] && CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
[[ -n "$ARCH" ]] && CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="$ARCH")

if [[ -n "$QTDIR" ]]; then
    CMAKE_ARGS+=(-DQt6_DIR="$QTDIR/lib/cmake/Qt6")
    CMAKE_ARGS+=(-DQt5_DIR="$QTDIR/lib/cmake/Qt5")
fi

if cmake "${CMAKE_ARGS[@]}"; then
    print_status "CMake configuration successful"
else
    print_error "CMake configuration failed"
    exit 1
fi

# Build
print_status "Building project..."
BUILD_ARGS=(--build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel)
if cmake "${BUILD_ARGS[@]}"; then
    print_status "Build successful"
else
    print_error "Build failed"
    exit 1
fi

# Copy library for tests (like Linux script does)
if [[ -f "$BUILD_DIR/$BUILD_TYPE/libQNetworkRequest.dylib" ]]; then
    print_status "Copying library for tests..."
    mkdir -p "$BUILD_DIR/test/$BUILD_TYPE"
    cp "$BUILD_DIR/$BUILD_TYPE/libQNetworkRequest.dylib" "$BUILD_DIR/test/$BUILD_TYPE/" 2>/dev/null || true
fi

# Tests
if [[ "$RUN_TESTS" == true ]]; then
    print_status "Running tests..."
    if cd "$BUILD_DIR"; then
        if ctest -C "$BUILD_TYPE" --output-on-failure; then
            print_status "All tests passed"
        else
            print_warning "Some tests failed"
        fi
        cd ..
    fi
fi

print_status "Build completed successfully!"
print_status "Executables located in: $BUILD_DIR/$BUILD_TYPE/"
print_status "Library file: $BUILD_DIR/$BUILD_TYPE/libQNetworkRequest.dylib"

if [[ "$BUILD_TYPE" == "Release" ]]; then
    print_status "Sample applications:"
    print_status "  - GUI demo: $BUILD_DIR/$BUILD_TYPE/QtRequester"
    print_status "  - Download manager: $BUILD_DIR/$BUILD_TYPE/QtDownloader"
fi

if [[ "$RUN_TESTS" == true ]]; then
    print_status "Unit tests: $BUILD_DIR/test/$BUILD_TYPE/UnitTests"
fi
