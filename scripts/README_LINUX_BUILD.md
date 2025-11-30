# Linux Build Guide

This document provides instructions for building QtMultiThreadNetwork on Linux systems.

## Prerequisites

### System Requirements
- **Linux Distribution**: Ubuntu 18.04+, Fedora 30+, Arch Linux, or similar
- **Qt Version**: Qt 5.6.3+ or Qt 6.x
- **CMake**: 3.15 or later
- **Compiler**: GCC 7+ or Clang 6+
- **Make**: GNU Make 3.8+

### Required Packages

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    qtbase5-dev \
    qtbase5-dev-tools \
    libqt5widgets5 \
    libqt5network5 \
    libqt5xml5 \
    qt5-qmake \
    libssl-dev
```

#### Fedora/CentOS/RHEL
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    qt5-qtbase-devel \
    qt5-qtbase-gui \
    qt5-qmake \
    openssl-devel
```

#### Arch Linux
```bash
sudo pacman -S \
    base-devel \
    cmake \
    qt5-base \
    openssl
```

#### openSUSE
```bash
sudo zypper install -y \
    gcc-c++ \
    cmake \
    libqt5-qtbase-devel \
    libqt5-qttools \
    openssl-devel
```

## Building the Project

### Using the Linux Build Script (Recommended)

The `build_linux.sh` script provides an easy way to build the project:

```bash
# Navigate to scripts directory
cd scripts

# Run the build script
./build_linux.sh

# Build with debug symbols
./build_linux.sh --debug

# Clean build before compiling
./build_linux.sh --clean

# Build and run tests
./build_linux.sh --tests

# Verbose output
./build_linux.sh --verbose

# Install to custom prefix
./build_linux.sh --prefix /usr/local
```

#### Script Options
- `--debug`: Build in Debug mode
- `--release`: Build in Release mode (default)
- `--relwithdebinfo`: Build with debug info in release
- `--minsizerel`: Build optimized for size
- `--clean`: Clean build directory before building
- `--qt-version VER`: Specify Qt version (5 or 6)
- `--prefix DIR`: Install prefix directory
- `--build-dir DIR`: Build directory name
- `--tests`: Run tests after building
- `-v, --verbose`: Verbose output
- `-h, --help`: Show help message

### Manual Build with CMake

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release

# Build project
cmake --build . --config Release --parallel

# Run tests (optional)
ctest -C Release --output-on-failure
```

### Manual Build with QMake

If you prefer QMake:

```bash
# Build the library
cd source
qmake QNetworkRequest.pro
make

# Build sample applications
cd ../samples/networkrequesttool
qmake networkrequesttool.pro
make

cd ../downloader
qmake downloader.pro
make
```

## Build Output

After building, you'll find the following files in the `build/Release/` directory:

- `libQNetworkRequest.so` - Main library
- `NetworkRequestTool` - GUI demo application
- `QtDownloader` - Download manager application
- `test/UnitTests` - Unit test executable

## Running the Applications

### Setting Library Path

Before running the applications, you need to set the library path:

```bash
export LD_LIBRARY_PATH=$PWD/build/Release:$LD_LIBRARY_PATH
```

### Running the GUI Demo
```bash
cd build/Release
./NetworkRequestTool
```

### Running the Download Manager
```bash
cd build/Release
./QtDownloader
```

### Running Tests
```bash
cd build/Release
./test/UnitTests
```

## Troubleshooting

### Qt Not Found
If you get Qt-related errors, ensure Qt development packages are installed:

```bash
# Check Qt installation
qmake --version
# or
qmake6 --version

# Find Qt paths
pkg-config --modpath Qt5Core --cflags
```

### OpenSSL Issues
If you encounter OpenSSL-related errors:

```bash
# Install OpenSSL development headers
sudo apt-get install libssl-dev  # Ubuntu/Debian
sudo dnf install openssl-devel   # Fedora
sudo pacman -S openssl           # Arch
```

### CMake Version Too Old
If CMake complains about version requirements:

```bash
# Update CMake
sudo apt-get install cmake      # Ubuntu/Debian
sudo dnf install cmake          # Fedora
sudo pacman -S cmake            # Arch
```

### Missing Dependencies
For missing Qt dependencies, you can use:

```bash
# Install all Qt5 development packages
sudo apt-get install qt5-default qtbase5-dev qttools5-dev-tools qtdeclarative5-dev
```

## Cross-Platform Considerations

### Windows-specific Code
The project contains Windows-specific code for memory-mapped files. This will be automatically excluded on Linux builds.

### OpenSSL
Linux systems use system OpenSSL libraries, so the Windows-specific OpenSSL DLL copying is not needed.

### Path Separators
The project uses Qt's path handling which automatically handles different path separators across platforms.

## Contributing

When contributing to this project, please ensure:

1. Your changes build on both Windows and Linux
2. Use cross-platform Qt APIs instead of platform-specific code
3. Test your changes with the provided unit tests
4. Update documentation if you add new features

## Support

For issues related to Linux builds, please check:

1. System requirements and dependencies
2. Qt installation and paths
3. Compiler and CMake versions
4. Library paths and runtime dependencies