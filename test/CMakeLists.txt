# ══════════════════════════════════════════════════════════════════════════════
# Unit and UI test executables
# ══════════════════════════════════════════════════════════════════════════════

# ── OpenSSL bin dir (Windows) ────────────────────────────────────────────────
if(WIN32)
    if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION_MINOR LESS 12)
        set(OPENSSL_BIN_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/openssl/1.0.2/bin")
    else()
        set(OPENSSL_BIN_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/openssl/1.1.1/bin")
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# UnitTests — core library functional tests
# ══════════════════════════════════════════════════════════════════════════════
add_executable(UnitTests
    main.cpp
    test_networkrequest.cpp
    httptestserver.cpp
)

target_link_libraries(UnitTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(UnitTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_output_directories(UnitTests)

if(WIN32)
    copy_openssl_dlls(UnitTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(UnitTests QNetworkRequest)
endif()

add_test(NAME UnitTests COMMAND UnitTests)
set_tests_properties(UnitTests PROPERTIES TIMEOUT 120)

# ══════════════════════════════════════════════════════════════════════════════
# BuilderTests — pure RequestContextBuilder fluent-API unit tests (no network)
# ══════════════════════════════════════════════════════════════════════════════
add_executable(BuilderTests
    main_requestbuilder.cpp
    test_requestbuilder.cpp
    test_bodytype.cpp
    test_environment.cpp
    httptestserver.cpp
)

target_link_libraries(BuilderTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(BuilderTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_output_directories(BuilderTests)

if(WIN32)
    copy_openssl_dlls(BuilderTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(BuilderTests QNetworkRequest)
endif()

add_test(NAME BuilderTests COMMAND BuilderTests)
set_tests_properties(BuilderTests PROPERTIES TIMEOUT 60)

# ══════════════════════════════════════════════════════════════════════════════
# CollectionTests — Collection model + Postman v2.1 converter (pure, no network)
# ══════════════════════════════════════════════════════════════════════════════
add_executable(CollectionTests
    main_collection.cpp
    test_collection.cpp
    test_postman.cpp
)

target_link_libraries(CollectionTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(CollectionTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_output_directories(CollectionTests)

if(WIN32)
    copy_openssl_dlls(CollectionTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(CollectionTests QNetworkRequest)
endif()

add_test(NAME CollectionTests COMMAND CollectionTests)
set_tests_properties(CollectionTests PROPERTIES TIMEOUT 60)

# ══════════════════════════════════════════════════════════════════════════════
# AuthTests — authentication system integration tests
# ══════════════════════════════════════════════════════════════════════════════
add_executable(AuthTests
    main_auth.cpp
    test_auth.cpp
    httptestserver.cpp
)

target_link_libraries(AuthTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(AuthTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_output_directories(AuthTests)

if(WIN32)
    copy_openssl_dlls(AuthTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(AuthTests QNetworkRequest)
endif()

add_test(NAME AuthTests COMMAND AuthTests)
set_tests_properties(AuthTests PROPERTIES TIMEOUT 120)

# ══════════════════════════════════════════════════════════════════════════════
# UtilityTests — pure NetworkRequestUtility unit tests (filesystem only)
#
# NetworkRequestUtility is an internal helper that is NOT exported from the
# shared library, so its implementation is compiled directly into this
# executable rather than linked from QNetworkRequest.
# ══════════════════════════════════════════════════════════════════════════════
add_executable(UtilityTests
    main_utility.cpp
    test_utility.cpp
    ${CMAKE_SOURCE_DIR}/source/networkrequestutility.cpp
)

target_link_libraries(UtilityTests
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(UtilityTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/source
)

set_output_directories(UtilityTests)

if(WIN32)
    copy_openssl_dlls(UtilityTests "${OPENSSL_BIN_DIR}")
endif()

add_test(NAME UtilityTests COMMAND UtilityTests)
set_tests_properties(UtilityTests PROPERTIES TIMEOUT 60)

# ══════════════════════════════════════════════════════════════════════════════
# DownloaderTests — pure NetworkDownloadTask value-type tests (networkdownloader
# sample). downloadtask.h is header-only, so only Qt is required and the sample
# directory is added to the include path to resolve the header.
# ══════════════════════════════════════════════════════════════════════════════
add_executable(DownloaderTests
    main_qtdownloader.cpp
    test_qtdownloader.cpp
)

target_link_libraries(DownloaderTests
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(DownloaderTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader
)

set_output_directories(DownloaderTests)

if(WIN32)
    copy_openssl_dlls(DownloaderTests "${OPENSSL_BIN_DIR}")
endif()

add_test(NAME DownloaderTests COMMAND DownloaderTests)
set_tests_properties(DownloaderTests PROPERTIES TIMEOUT 60)

# ══════════════════════════════════════════════════════════════════════════════
# UiTests — QtRequester GUI automation tests
# ══════════════════════════════════════════════════════════════════════════════
add_executable(UiTests
    main_uitester.cpp
    test_qtrequester.cpp
    httptestserver.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/networkrequesttool.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/jsonsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/xmlsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/thememanager.h
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/NetworkRequestTool.ui
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/NetworkRequestTool.qrc
)

target_compile_definitions(UiTests PRIVATE QT_MTNETWORK_UNIT_TEST)

target_link_libraries(UiTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(UiTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool
)

set_output_directories(UiTests)

if(WIN32)
    copy_openssl_dlls(UiTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(UiTests QNetworkRequest)
endif()

add_test(NAME UiTests COMMAND UiTests)
set_tests_properties(UiTests PROPERTIES TIMEOUT 120)

# ══════════════════════════════════════════════════════════════════════════════
# ThemeTests — QtRequester light/dark/system theming (GUI)
# ══════════════════════════════════════════════════════════════════════════════
add_executable(ThemeTests
    main_theme.cpp
    test_theme.cpp
    httptestserver.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/networkrequesttool.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/jsonsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/xmlsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/thememanager.h
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/NetworkRequestTool.ui
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/NetworkRequestTool.qrc
)

target_compile_definitions(ThemeTests PRIVATE QT_MTNETWORK_UNIT_TEST)

target_link_libraries(ThemeTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(ThemeTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool
)

set_output_directories(ThemeTests)

if(WIN32)
    copy_openssl_dlls(ThemeTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(ThemeTests QNetworkRequest)
endif()

add_test(NAME ThemeTests COMMAND ThemeTests)
set_tests_properties(ThemeTests PROPERTIES TIMEOUT 120)

# ══════════════════════════════════════════════════════════════════════════════
# DownloaderUiTests — QtDownloader light/dark rendering + View-menu theme (GUI)
#
# The existing DownloaderTests suite is non-GUI (QCoreApplication) and cannot
# exercise rendering, so this suite uses QApplication and constructs the full
# NetworkDownloaderMainWindow.
# ══════════════════════════════════════════════════════════════════════════════
add_executable(DownloaderUiTests
    main_downloaderui.cpp
    test_downloaderui.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/downloadermainwindow.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/downloadtaskmodel.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/downloadmanager.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/tasktabledelegate.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/thememanager.h
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/NetworkDownloaderMainWindow.ui
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader/NetworkDownloader.qrc
)

target_link_libraries(DownloaderUiTests
    QNetworkRequest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(DownloaderUiTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/samples/networkdownloader
)

set_output_directories(DownloaderUiTests)

if(WIN32)
    copy_openssl_dlls(DownloaderUiTests "${OPENSSL_BIN_DIR}")
    copy_library_artifact(DownloaderUiTests QNetworkRequest)
endif()

add_test(NAME DownloaderUiTests COMMAND DownloaderUiTests)
set_tests_properties(DownloaderUiTests PROPERTIES TIMEOUT 120)
