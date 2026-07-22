# ══════════════════════════════════════════════════════════════════════════════
# vcpkg portfile for QtNetworkRequest
#
# Triplets handle VS version + arch combos:
#   x64-windows        → VS 2022,  x64, dynamic CRT
#   x64-windows-static →            x64, static  CRT (/MT)
#   x86-windows        →            x86, dynamic CRT  (blocked by "supports")
#
# Features handle Qt version:
#   qtnetworkrequest[qt5]   → Qt 5.x   (default)
#   qtnetworkrequest[qt6]   → Qt 6.x
#
# C++ source uses QT_VERSION preprocessor checks → binary changes per Qt ver.
# ══════════════════════════════════════════════════════════════════════════════

vcpkg_check_linkage(ONLY_DYNAMIC_CRT)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO your-org/qt-network-request
    REF "v${VERSION}"
    SHA512 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    HEAD_REF master
)

# ── Feature → CMake option mapping ────────────────────────────────────────────
vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        qt6    USE_QT6
)

# ── Configure ─────────────────────────────────────────────────────────────────
set(CONFIGURE_OPTIONS
    -DBUILD_TESTS=OFF
    -DBUILD_SAMPLES=OFF
)

# Qt6 needs explicit find_package component selection
if(USE_QT6)
    list(APPEND CONFIGURE_OPTIONS -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${CONFIGURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    PACKAGE_NAME QtNetworkRequest
    CONFIG_PATH lib/cmake/QtNetworkRequest
)
vcpkg_copy_pdbs()

# ── License ───────────────────────────────────────────────────────────────────
file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
     RENAME copyright)

# ── Usage hint ────────────────────────────────────────────────────────────────
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage"
[=[
qtnetworkrequest provides CMake targets:

    find_package(QtNetworkRequest CONFIG REQUIRED)
    target_link_libraries(main PRIVATE QtNetworkRequest::QNetworkRequest)

    # Qt5
    find_package(Qt5 REQUIRED COMPONENTS Core Network)
    target_link_libraries(main PRIVATE Qt5::Core Qt5::Network)

    # Qt6
    find_package(Qt6 REQUIRED COMPONENTS Core Network)
    target_link_libraries(main PRIVATE Qt6::Core Qt6::Network)

Installed files:
    include/   — 9 public headers
    lib/       — QNetworkRequest.lib (import lib), cmake config
    bin/       — QNetworkRequest.dll (shared build)
]=])
