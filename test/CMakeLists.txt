# CMakeLists.txt for test directory
add_executable(UnitTests
    main.cpp
    test_networkrequest.cpp
    httptestserver.cpp
)

target_link_libraries(UnitTests 
    QNetworkRequest 
    Qt5::Core 
    Qt5::Network 
    Qt5::Test
)

target_include_directories(UnitTests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Register test with CTest
add_test(NAME UnitTests COMMAND UnitTests)

# --- Copy OpenSSL binary files to build directory ---
if(WIN32)
    if(Qt5_VERSION_MAJOR GREATER_EQUAL 5 AND Qt5_VERSION_MINOR GREATER_EQUAL 12)
        set(OPENSSL_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/openssl/1.1.1/bin")
    else()
        set(OPENSSL_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/openssl/1.0.2/bin")
    endif()
    
    if(Qt5_VERSION_MAJOR GREATER_EQUAL 5 AND Qt5_VERSION_MINOR GREATER_EQUAL 12)
        set(OPENSSL_DLLS libcrypto-1_1-x64.dll libssl-1_1-x64.dll)
    else()
        set(OPENSSL_DLLS libeay32.dll ssleay32.dll)
    endif()
    
    foreach(dll ${OPENSSL_DLLS})
        add_custom_command(
            TARGET UnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/${dll}"
            $<TARGET_FILE_DIR:UnitTests>
        )
    endforeach()

    # Copy QNetworkRequest DLL to test directory
    add_custom_command(
        TARGET UnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:QNetworkRequest>/$<TARGET_FILE_NAME:QNetworkRequest>"
        $<TARGET_FILE_DIR:UnitTests>
    )

endif()

set_tests_properties(UnitTests PROPERTIES TIMEOUT 120)

# ─── UiTests: QtRequester GUI tests ────────────────────────────────────────
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
    Qt5::Core
    Qt5::Network
    Qt5::Widgets
    Qt5::Xml
    Qt5::Test
)

target_include_directories(UiTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/samples/networkrequesttool
)

if(WIN32)
    foreach(dll ${OPENSSL_DLLS})
        add_custom_command(
            TARGET UiTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/${dll}"
            $<TARGET_FILE_DIR:UiTests>
        )
    endforeach()

    add_custom_command(
        TARGET UiTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:QNetworkRequest>/$<TARGET_FILE_NAME:QNetworkRequest>"
        $<TARGET_FILE_DIR:UiTests>
    )
endif()

add_test(NAME UiTests COMMAND UiTests)
set_tests_properties(UiTests PROPERTIES TIMEOUT 120)