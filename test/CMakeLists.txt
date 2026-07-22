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
# UiTests — QtRequester GUI automation tests
# ══════════════════════════════════════════════════════════════════════════════
add_executable(UiTests
    main_uitester.cpp
    test_qtrequester.cpp
    httptestserver.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/networkrequesttool.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/jsonsyntaxhighlighter.cpp
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool/xmlsyntaxhighlighter.cpp
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
