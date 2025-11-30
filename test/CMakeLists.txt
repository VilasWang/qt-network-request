# CMakeLists.txt for test directory
add_executable(UnitTests
    main.cpp
    test_networkrequest.cpp
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

# --- Copy OpenSSL binary files to build directory ---
if(WIN32)
    # Set OpenSSL binary file path
    if(Qt5_VERSION_MAJOR GREATER_EQUAL 5 AND Qt5_VERSION_MINOR GREATER_EQUAL 12)
        set(OPENSSL_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/openssl/1.1.1/bin")
    else()
        set(OPENSSL_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/openssl/1.0.2/bin")
    endif()
    
    # Copy OpenSSL DLL files to build directory
    if(Qt5_VERSION_MAJOR GREATER_EQUAL 5 AND Qt5_VERSION_MINOR GREATER_EQUAL 12)
        add_custom_command(
            TARGET UnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libcrypto-1_1-x64.dll"
            "${OPENSSL_BIN_DIR}/libssl-1_1-x64.dll"
            $<TARGET_FILE_DIR:UnitTests>
        )
    else()
        add_custom_command(
            TARGET UnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libeay32.dll"
            "${OPENSSL_BIN_DIR}/ssleay32.dll"
            $<TARGET_FILE_DIR:UnitTests>
        )
    endif()

    add_custom_command(
        TARGET UnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/$<CONFIG>/QNetworkRequest.dll"
        $<TARGET_FILE_DIR:UnitTests>
    )

endif()