# ══════════════════════════════════════════════════════════════════════════════
# Sample GUI applications: QtRequester + QtDownloader
# ══════════════════════════════════════════════════════════════════════════════

# ── GUI executable flag (Windows: no console window) ─────────────────────────
if(WIN32)
    set(GUI_EXECUTABLE_FLAG WIN32)
else()
    set(GUI_EXECUTABLE_FLAG "")
endif()

# ── OpenSSL bin dir (Windows) ────────────────────────────────────────────────
if(WIN32)
    if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION_MINOR LESS 12)
        set(OPENSSL_BIN_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/openssl/1.0.2/bin")
    else()
        set(OPENSSL_BIN_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/openssl/1.1.1/bin")
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# QtRequester — HTTP request debugging tool
# ══════════════════════════════════════════════════════════════════════════════
add_executable(QtRequester ${GUI_EXECUTABLE_FLAG}
    networkrequesttool/main.cpp
    networkrequesttool/networkrequesttool.cpp
    networkrequesttool/jsonsyntaxhighlighter.cpp
    networkrequesttool/xmlsyntaxhighlighter.cpp

    # Headers (AUTOMOC)
    networkrequesttool/networkrequesttool.h
    networkrequesttool/jsonsyntaxhighlighter.h
    networkrequesttool/xmlsyntaxhighlighter.h

    # UI / Resources
    networkrequesttool/NetworkRequestTool.ui
    networkrequesttool/NetworkRequestTool.qrc
)

target_link_libraries(QtRequester PRIVATE
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Network
)
target_include_directories(QtRequester PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/networkrequesttool)
set_output_directories(QtRequester)

if(WIN32)
    copy_openssl_dlls(QtRequester "${OPENSSL_BIN_DIR}")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# QtDownloader — multi-threaded download manager
# ══════════════════════════════════════════════════════════════════════════════
add_executable(QtDownloader ${GUI_EXECUTABLE_FLAG}
    networkdownloader/main.cpp
    networkdownloader/downloadermainwindow.cpp
    networkdownloader/downloadtaskmodel.cpp
    networkdownloader/downloadmanager.cpp

    # Headers (AUTOMOC)
    networkdownloader/downloadermainwindow.h
    networkdownloader/downloadtaskmodel.h
    networkdownloader/downloadmanager.h
    networkdownloader/downloadtask.h

    # UI / Resources
    networkdownloader/NetworkDownloaderMainWindow.ui
    networkdownloader/NetworkDownloader.qrc
)

target_link_libraries(QtDownloader PRIVATE
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)
target_include_directories(QtDownloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/networkdownloader
    ${CMAKE_SOURCE_DIR}/include
)
set_output_directories(QtDownloader)

if(WIN32)
    copy_openssl_dlls(QtDownloader "${OPENSSL_BIN_DIR}")
endif()

# Additional MSVC debug settings (historically required for this target)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(QtDownloader PRIVATE /ZI /Od /RTC1 /Ob0)
    set_target_properties(QtDownloader PROPERTIES
        LINK_FLAGS "/DEBUG /INCREMENTAL"
        COMPILE_DEFINITIONS "_DEBUG"
        MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL"
    )
    target_compile_definitions(QtDownloader PRIVATE _DEBUG)
endif()
