# ══════════════════════════════════════════════════════════════════════════════
# QtMultiThreadNetwork — Root CMakeLists.txt
# ══════════════════════════════════════════════════════════════════════════════
cmake_minimum_required(VERSION 3.15)
project(QtMultiThreadNetwork VERSION 2.0.0 LANGUAGES CXX)

# ── C++ standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt meta-object compilation ───────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ── Include cmake modules ────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compiler-flags)
include(utilities)
include(openssl)

# ── Options ──────────────────────────────────────────────────────────────────
option(BUILD_TESTS   "Build unit and UI test executables" ON)
option(BUILD_SAMPLES "Build sample GUI applications"       ON)

# ── Find Qt ──────────────────────────────────────────────────────────────────
find_package(Qt5 COMPONENTS Core Widgets Network Xml Test REQUIRED)

if(Qt5_FOUND)
    message(STATUS "Qt version: ${Qt5_VERSION}")
    message(STATUS "Qt major version: ${Qt5_VERSION_MAJOR}")
    message(STATUS "Qt minor version: ${Qt5_VERSION_MINOR}")
endif()

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(source)

if(BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
