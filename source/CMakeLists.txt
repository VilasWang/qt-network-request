# ══════════════════════════════════════════════════════════════════════════════
# QNetworkRequest shared library
# ══════════════════════════════════════════════════════════════════════════════

add_library(QNetworkRequest SHARED
    # ── Implementation ───────────────────────────────────────────────────────
    memorymappedfile.cpp
    networkaccessmanagerpool.cpp
    networkcommonrequest.cpp
    networkcookiejar.cpp
    networkdownloadrequest.cpp
    networkmtdownloadrequest.cpp
    networkmtdownloadrequest_states.cpp
    networkreply.cpp
    networkrequest.cpp
    networkrequestmanager.cpp
    networkrequestrunnable.cpp
    networkuploadrequest.cpp
    networkrequestutility.cpp
    progressthrottle.cpp
    networkrequestregistry.cpp
    requeststrategies.cpp
    environment.cpp
    oauth2tokencache.cpp
    collectionmodel.cpp
    postmanconverter.cpp
    environmentstore.cpp

    # ── Private headers (AUTOMOC) ────────────────────────────────────────────
    memorymappedfile.h
    networkaccessmanagerpool.h
    networkcommonrequest.h
    networkcookiejar.h
    networkdownloadrequest.h
    networkmtdownloadrequest.h
    networkmtdownloadrequest_p.h
    networkrequest.h
    networkrequestrunnable.h
    networkuploadrequest.h
    networkrequestutility.h
    sharedcookiejar.h
    networkrequestregistry.h
    progressthrottle.h
    qtcompat.h
    requeststrategies.h

    # ── Public headers (AUTOMOC) ─────────────────────────────────────────────
    ${CMAKE_SOURCE_DIR}/include/networkrequestmanager.h
    ${CMAKE_SOURCE_DIR}/include/networkreply.h
    ${CMAKE_SOURCE_DIR}/include/taskdata.h
    ${CMAKE_SOURCE_DIR}/include/sslconfig.h
    ${CMAKE_SOURCE_DIR}/include/proxyconfig.h
    ${CMAKE_SOURCE_DIR}/include/requestcontext.h
    ${CMAKE_SOURCE_DIR}/include/authconfig.h
    ${CMAKE_SOURCE_DIR}/include/responseresult.h
    ${CMAKE_SOURCE_DIR}/include/networkerror.h
    ${CMAKE_SOURCE_DIR}/include/networkrequestglobal.h
    ${CMAKE_SOURCE_DIR}/include/networkrequestevent.h
    ${CMAKE_SOURCE_DIR}/include/environment.h
    ${CMAKE_SOURCE_DIR}/include/oauth2tokencache.h
    ${CMAKE_SOURCE_DIR}/include/collectionmodel.h
    ${CMAKE_SOURCE_DIR}/include/postmanconverter.h
    ${CMAKE_SOURCE_DIR}/include/environmentstore.h
)

# ── Compile definitions ──────────────────────────────────────────────────────
target_compile_definitions(QNetworkRequest
    PRIVATE
        QT_MTNETWORK_LIB
        $<$<CONFIG:Debug>:QT_DEBUG>
        $<$<CONFIG:RelWithDebInfo>:NDEBUG>
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CONFIG:MinSizeRel>:NDEBUG>
)

# ── Include directories ──────────────────────────────────────────────────────
target_include_directories(QNetworkRequest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ── Dependencies ─────────────────────────────────────────────────────────────
target_link_libraries(QNetworkRequest PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

# ── Output directories ───────────────────────────────────────────────────────
set_output_directories(QNetworkRequest)

# ── OpenSSL ──────────────────────────────────────────────────────────────────
# Windows: copy DLLs post-build
if(WIN32)
    if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION_MINOR LESS 12)
        set(OPENSSL_BIN_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/openssl/1.0.2/bin")
    else()
        set(OPENSSL_BIN_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/openssl/1.1.1/bin")
    endif()
    copy_openssl_dlls(QNetworkRequest "${OPENSSL_BIN_DIR}")
endif()

# Unix (Linux / macOS): find and link
setup_openssl_unix(QNetworkRequest)

# ── Install rules ────────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS QNetworkRequest
    EXPORT QtNetworkRequestTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT QtNetworkRequestTargets
    FILE QtNetworkRequestTargets.cmake
    NAMESPACE QtNetworkRequest::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtNetworkRequest
)

set(PROJECT_VERSION_COMPATIBILITY SameMajorVersion)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/QtNetworkRequestConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${PROJECT_VERSION_COMPATIBILITY}
)

set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
set(LIB_INSTALL_DIR    "${CMAKE_INSTALL_LIBDIR}")
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/QtNetworkRequestConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/QtNetworkRequestConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/QtNetworkRequest"
    PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/QtNetworkRequestConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/QtNetworkRequestConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/QtNetworkRequest"
)
