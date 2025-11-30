#!/bin/bash

# Linux build script for QtMultiThreadNetwork
# This script builds the project on Linux systems

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN_BUILD=false
QT_VERSION=""
VERBOSE=false
INSTALL_PREFIX=""
RUN_TESTS=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --relwithdebinfo)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        --minsizerel)
            BUILD_TYPE="MinSizeRel"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --qt-version)
            QT_VERSION="$2"
            shift 2
            ;;
        --prefix)
            INSTALL_PREFIX="-DCMAKE_INSTALL_PREFIX=$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --tests)
            RUN_TESTS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
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
            echo "  --tests             Run tests after building"
            echo "  -v, --verbose       Verbose output"
            echo "  -h, --help           Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_status "Starting Linux build for QtMultiThreadNetwork"
print_status "Build type: $BUILD_TYPE"
print_status "Build directory: $BUILD_DIR"

# Check if we're in the scripts directory
if [[ "$(basename "$(pwd)")" != "scripts" ]]; then
    print_error "Please run this script from the 'scripts' directory"
    exit 1
fi

# Navigate to project root
cd ..

# Check for Qt installation
if [[ -n "$QT_VERSION" ]]; then
    if [[ "$QT_VERSION" == "6" ]]; then
        QT_PACKAGES="Qt6"
        QT_COMPONENTS="Core Widgets Network Xml Test"
    else
        QT_PACKAGES="Qt5"
        QT_COMPONENTS="Core Widgets Network Xml Test"
    fi
else
    # Auto-detect Qt version
    if command -v qmake6 &> /dev/null; then
        QT_PACKAGES="Qt6"
        QT_COMPONENTS="Core Widgets Network Xml Test"
        print_status "Detected Qt6"
    elif command -v qmake &> /dev/null; then
        QT_PACKAGES="Qt5"
        QT_COMPONENTS="Core Widgets Network Xml Test"
        print_status "Detected Qt5"
    else
        print_error "Qt installation not found. Please install Qt development packages."
        print_error "For Ubuntu/Debian: sudo apt-get install qtbase5-dev qtbase5-dev-tools libqt5widgets5 libqt5network5 libqt5xml5 qt5-qmake"
        print_error "For Fedora: sudo dnf install qt5-qtbase-devel qt5-qtbase-gui qt5-qmake"
        print_error "For Arch: sudo pacman -S qt5-base"
        exit 1
    fi
fi

# Check for required system packages
print_status "Checking system dependencies..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Please install CMake (>= 3.15)."
    exit 1
fi

# Check for C++ compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    print_error "C++ compiler not found. Please install gcc-c++ or clang++."
    exit 1
fi

# Check for make
if ! command -v make &> /dev/null; then
    print_error "Make not found. Please install build-essential or make."
    exit 1
fi

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    print_status "Cleaning build directory..."
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
    fi
fi

# Create build directory
if [[ ! -d "$BUILD_DIR" ]]; then
    mkdir -p "$BUILD_DIR"
fi

# Configure project with CMake
print_status "Configuring project with CMake..."

CMAKE_ARGS=(
    -S .
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    $INSTALL_PREFIX
)

# Add verbose flag if requested
if [[ "$VERBOSE" == true ]]; then
    CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
fi

# Check if we need to specify Qt path
if [[ -n "$QTDIR" ]]; then
    CMAKE_ARGS+=(-DQt6_DIR="$QTDIR/lib/cmake/Qt6")
    CMAKE_ARGS+=(-DQt5_DIR="$QTDIR/lib/cmake/Qt5")
fi

# Run CMake configuration
if cmake "${CMAKE_ARGS[@]}"; then
    print_status "CMake configuration successful"
else
    print_error "CMake configuration failed"
    exit 1
fi

# Build project
print_status "Building project..."

BUILD_ARGS=(
    --build "$BUILD_DIR"
    --config "$BUILD_TYPE"
    --parallel
)

if cmake "${BUILD_ARGS[@]}"; then
    print_status "Build successful"
else
    print_error "Build failed"
    exit 1
fi

# Copy library for tests (Linux equivalent of Windows DLL copying)
if [[ -f "$BUILD_DIR/$BUILD_TYPE/libQNetworkRequest.so" ]]; then
    print_status "Copying library for tests..."
    cp "$BUILD_DIR/$BUILD_TYPE/libQNetworkRequest.so" "$BUILD_DIR/test/$BUILD_TYPE/" 2>/dev/null || true
fi

# Run tests if requested
if [[ "$RUN_TESTS" == true ]]; then
    print_status "Running tests..."
    if cd "$BUILD_DIR"; then
        if ctest -C "$BUILD_TYPE" --output-on-failure; then
            print_status "All tests passed"
        else
            print_warning "Some tests failed"
        fi
        cd ..
    else
        print_warning "Could not run tests - build directory not found"
    fi
fi

print_status "Build completed successfully!"
print_status "Executables are located in: $BUILD_DIR/$BUILD_TYPE/"
print_status "Library file: $BUILD_DIR/$BUILD_TYPE/libQNetworkRequest.so"

if [[ "$BUILD_TYPE" == "Release" ]]; then
    print_status "Sample applications:"
    print_status "  - GUI demo: $BUILD_DIR/$BUILD_TYPE/NetworkRequestTool"
    print_status "  - Download manager: $BUILD_DIR/$BUILD_TYPE/QtDownloader"
fi

if [[ "$RUN_TESTS" == true ]]; then
    print_status "Unit tests: $BUILD_DIR/test/$BUILD_TYPE/UnitTests"
fi

print_status "To run the applications, ensure Qt libraries are in your LD_LIBRARY_PATH or install them system-wide."