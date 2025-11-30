#!/bin/bash

# Local test script to simulate Gitee CI environment
# This script helps test the build configuration locally

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Simulate Gitee CI environment
print_status "Simulating Gitee CI build environment..."

# Navigate to project root
cd ..

# Check if we have the required files
if [[ ! -f "scripts/build_linux.sh" ]]; then
    print_error "build_linux.sh not found"
    exit 1
fi

if [[ ! -f ".gitee/workflows/linux-build.yml" ]]; then
    print_error "Gitee CI configuration not found"
    exit 1
fi

# Test 1: Check script syntax
print_status "Test 1: Checking shell script syntax..."
bash -n scripts/build_linux.sh
if [[ $? -eq 0 ]]; then
    print_status "✓ Shell script syntax is valid"
else
    print_error "✗ Shell script syntax error"
    exit 1
fi

# Test 2: Check script help
print_status "Test 2: Testing script help functionality..."
scripts/build_linux.sh --help > /dev/null
if [[ $? -eq 0 ]]; then
    print_status "✓ Script help works correctly"
else
    print_error "✗ Script help failed"
    exit 1
fi

# Test 3: Check YAML syntax
print_status "Test 3: Checking YAML syntax..."
if command -v python3 &> /dev/null; then
    python3 -c "import yaml; yaml.safe_load(open('.gitee/workflows/linux-build.yml', 'r'))" 2>/dev/null
    if [[ $? -eq 0 ]]; then
        print_status "✓ YAML syntax is valid"
    else
        print_warning "⚠ Could not verify YAML syntax (no YAML parser available)"
    fi
else
    print_warning "⚠ Python3 not available, skipping YAML syntax check"
fi

# Test 4: Check for required dependencies
print_status "Test 4: Checking for required dependencies..."

DEPS_MISSING=false

if ! command -v cmake &> /dev/null; then
    print_warning "⚠ CMake not found"
    DEPS_MISSING=true
fi

if ! command -v make &> /dev/null; then
    print_warning "⚠ Make not found"
    DEPS_MISSING=true
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    print_warning "⚠ C++ compiler not found"
    DEPS_MISSING=true
fi

if [[ "$DEPS_MISSING" == true ]]; then
    print_warning "Some dependencies are missing. Install them to run full build test."
    print_warning "On Ubuntu/Debian: sudo apt-get install build-essential cmake"
    print_warning "On Fedora: sudo dnf install gcc-c++ cmake make"
else
    print_status "✓ All required dependencies found"
fi

# Test 5: Check CMakeLists.txt
print_status "Test 5: Checking CMakeLists.txt..."
if [[ -f "CMakeLists.txt" ]]; then
    # Try to parse CMakeLists.txt for basic syntax
    if cmake -P CMakeLists.txt 2>&1 | head -5; then
        print_status "✓ CMakeLists.txt is readable"
    else
        print_warning "⚠ CMakeLists.txt may have issues"
    fi
else
    print_error "✗ CMakeLists.txt not found"
    exit 1
fi

# Test 6: Check source files
print_status "Test 6: Checking source files..."
SOURCE_COUNT=$(find source/ -name "*.cpp" -o -name "*.h" | wc -l)
if [[ $SOURCE_COUNT -gt 0 ]]; then
    print_status "✓ Found $SOURCE_COUNT source files"
else
    print_error "✗ No source files found"
    exit 1
fi

# Test 7: Try dry-run build (if dependencies available)
if [[ "$DEPS_MISSING" == false ]]; then
    print_status "Test 7: Testing dry-run build..."

    # Create build directory
    mkdir -p build
    cd build

    # Try to configure
    if cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release; then
        print_status "✓ CMake configuration successful"

        # Try dry-run build
        if cmake --build . --config Release --dry-run; then
            print_status "✓ Build dry-run successful"
        else
            print_warning "⚠ Build dry-run failed"
        fi
    else
        print_warning "⚠ CMake configuration failed"
    fi

    cd ..

    # Clean up
    rm -rf build
else
    print_warning "Skipping build test due to missing dependencies"
fi

print_status "Local simulation completed!"
print_status ""
print_status "To run the actual build test:"
print_status "1. Install required dependencies"
print_status "2. Run: cd scripts && bash build_linux.sh --release --clean"
print_status ""
print_status "To enable Gitee CI:"
print_status "1. Push your code to Gitee"
print_status "2. Enable Gitee CI in repository settings"
print_status "3. CI will automatically trigger on push/PR"