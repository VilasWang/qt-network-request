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
# Try Qt6 first, fall back to Qt5 (Qt6 not fully supported yet, but infra ready)
option(USE_QT6 "Build against Qt6 instead of Qt5" OFF)

if(USE_QT6)
    find_package(Qt6 COMPONENTS Core Widgets Network Xml Test REQUIRED)
    set(QT_VERSION_MAJOR 6)
else()
    find_package(Qt5 5.9 COMPONENTS Core Widgets Network Xml Test REQUIRED)
    set(QT_VERSION_MAJOR 5)
endif()

message(STATUS "Qt version: ${Qt${QT_VERSION_MAJOR}_VERSION}")
message(STATUS "Qt major version: ${QT_VERSION_MAJOR}")

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(source)

if(BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
