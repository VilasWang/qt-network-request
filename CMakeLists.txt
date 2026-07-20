cmake_minimum_required(VERSION 3.15)
project(QtMultiThreadNetwork LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically handle Qt's moc, uic, rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Specify MSVC runtime library
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # Debug configuration settings
    set(CMAKE_CXX_FLAGS_DEBUG "/ZI /Od /MDd /RTC1 /GS /Ob0")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL")
    
    # Release configuration settings
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MD")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
    
    # RelWithDebInfo configuration settings (Release with debug info)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /MD")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
    
    # MinSizeRel configuration settings (minimum size Release)
    set(CMAKE_CXX_FLAGS_MINSIZEREL "/O1 /MD")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
endif()

# Find Qt libraries
find_package(Qt5 COMPONENTS Core Widgets Network Xml Test REQUIRED)

if(Qt5_FOUND)
    message(STATUS "Qt version: ${Qt5_VERSION}")
    message(STATUS "Qt major version: ${Qt5_VERSION_MAJOR}")
    message(STATUS "Qt minor version: ${Qt5_VERSION_MINOR}")
endif()

# --- Build library: QNetworkRequest ---
add_library(QNetworkRequest SHARED
    # Sources
    source/memorymappedfile.cpp
    source/networkcookiejar.cpp
    source/networkcommonrequest.cpp
    source/networkdownloadrequest.cpp
    source/networkrequestmanager.cpp
    source/networkmtdownloadrequest.cpp
    source/networkreply.cpp
    source/networkrequest.cpp
    source/networkrequestrunnable.cpp
    source/networkuploadrequest.cpp
    source/networkrequestutility.cpp

    # Headers for AUTOMOC
    include/networkrequestmanager.h
    include/networkreply.h
    include/networkrequestdefs.h
    include/networkrequestglobal.h
    source/memorymappedfile.h
    source/networkcookiejar.h
    source/networkrequestevent.h
    source/networkcommonrequest.h
    source/networkdownloadrequest.h
    source/networkmtdownloadrequest.h
    source/networkrequest.h
    source/networkrequestrunnable.h
    source/networkuploadrequest.h
    source/networkrequestutility.h
)
target_compile_definitions(QNetworkRequest 
    PRIVATE 
        QT_MTNETWORK_LIB
        $<$<CONFIG:Debug>:QT_DEBUG>
        $<$<CONFIG:RelWithDebInfo>:NDEBUG>
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CONFIG:MinSizeRel>:NDEBUG>
)

# Set include directories for library
target_include_directories(QNetworkRequest
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# Link Qt modules required by library
target_link_libraries(QNetworkRequest PRIVATE Qt5::Core Qt5::Network)

# --- Platform-specific GUI executable flag ---
# WIN32 flag is only valid on Windows for GUI applications (no console window)
if(WIN32)
    set(GUI_EXECUTABLE_FLAG WIN32)
else()
    set(GUI_EXECUTABLE_FLAG "")
endif()

# --- Build executable: NetworkRequestTool ---
add_executable(QtRequester ${GUI_EXECUTABLE_FLAG}
    # Sources
    samples/networkrequesttool/main.cpp
    samples/networkrequesttool/networkrequesttool.cpp

    # Headers for AUTOMOC
    samples/networkrequesttool/networkrequesttool.h

    # UI and Resource files
    samples/networkrequesttool/NetworkRequestTool.ui
    samples/networkrequesttool/NetworkRequestTool.qrc
)

# Link NetworkRequestTool to our library and Qt modules
target_link_libraries(QtRequester PRIVATE QNetworkRequest Qt5::Widgets Qt5::Xml Qt5::Network)

# Set include directories for executable
target_include_directories(QtRequester PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/samples/networkrequesttool)

# --- Build executable: NetworkDownloader ---
add_executable(QtDownloader ${GUI_EXECUTABLE_FLAG}
    # Sources
    samples/networkdownloader/main.cpp
    samples/networkdownloader/downloadermainwindow.cpp
    samples/networkdownloader/downloadtaskmodel.cpp
    samples/networkdownloader/downloadmanager.cpp

    # Headers for AUTOMOC
    samples/networkdownloader/downloadermainwindow.h
    samples/networkdownloader/downloadtaskmodel.h
    samples/networkdownloader/downloadmanager.h
    samples/networkdownloader/downloadtask.h

    # UI and Resource files
    samples/networkdownloader/NetworkDownloaderMainWindow.ui
    samples/networkdownloader/NetworkDownloader.qrc
)

# Add additional debug settings for QtDownloader
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(QtDownloader PRIVATE /ZI /Od /RTC1 /Ob0)
    set_target_properties(QtDownloader PROPERTIES
        LINK_FLAGS "/DEBUG /INCREMENTAL"
        COMPILE_DEFINITIONS "_DEBUG"
        MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL"
    )
    # Ensure debug information is generated
    target_compile_definitions(QtDownloader PRIVATE _DEBUG)
endif()

# Link downloader to our library and Qt modules
target_link_libraries(QtDownloader PRIVATE QNetworkRequest Qt5::Widgets Qt5::Network)

# Set include directories for executable
target_include_directories(QtDownloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/samples/networkdownloader
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Set output directories - QNetworkRequest library
set_target_properties(QNetworkRequest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
)

# Set output directories - QtDownloader
set_target_properties(QtDownloader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
)

# Set output directories - QtRequester
set_target_properties(QtRequester PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/build/Release"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/build/RelWithDebInfo"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/build/MinSizeRel"
)

# --- Copy OpenSSL binary files to build directory ---
if(WIN32)
    # Set OpenSSL binary file path
    if(Qt5_VERSION_MAJOR GREATER_EQUAL 5 AND Qt5_VERSION_MINOR GREATER_EQUAL 12)
        set(OPENSSL_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/openssl/1.1.1/bin")
    else()
        set(OPENSSL_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/openssl/1.0.2/bin")
    endif()
    
    # Copy OpenSSL DLL files to build directory
    if(Qt5_VERSION_MAJOR GREATER_EQUAL 5 AND Qt5_VERSION_MINOR GREATER_EQUAL 12)
        add_custom_command(
            TARGET QNetworkRequest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libcrypto-1_1-x64.dll"
            "${OPENSSL_BIN_DIR}/libssl-1_1-x64.dll"
            $<TARGET_FILE_DIR:QNetworkRequest>
        )
        
        add_custom_command(
            TARGET QtRequester POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libcrypto-1_1-x64.dll"
            "${OPENSSL_BIN_DIR}/libssl-1_1-x64.dll"
            $<TARGET_FILE_DIR:QtRequester>
        )
        
        add_custom_command(
            TARGET QtDownloader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libcrypto-1_1-x64.dll"
            "${OPENSSL_BIN_DIR}/libssl-1_1-x64.dll"
            $<TARGET_FILE_DIR:QtDownloader>
        )
    else()
        add_custom_command(
            TARGET QNetworkRequest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libeay32.dll"
            "${OPENSSL_BIN_DIR}/ssleay32.dll"
            $<TARGET_FILE_DIR:QNetworkRequest>
        )
        
        add_custom_command(
            TARGET QtRequester POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libeay32.dll"
            "${OPENSSL_BIN_DIR}/ssleay32.dll"
            $<TARGET_FILE_DIR:QtRequester>
        )
        
        add_custom_command(
            TARGET QtDownloader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_BIN_DIR}/libeay32.dll"
            "${OPENSSL_BIN_DIR}/ssleay32.dll"
            $<TARGET_FILE_DIR:QtDownloader>
        )
    endif()
endif()

# --- OpenSSL for Unix-like systems (Linux, macOS) ---
if(UNIX AND NOT APPLE)
    # Linux: Find system OpenSSL packages
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
        message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
        message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")

        # Link OpenSSL to the library
        target_link_libraries(QNetworkRequest PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    else()
        message(FATAL_ERROR "OpenSSL not found. Install libssl-dev on Linux:\n"
            "  Ubuntu/Debian: sudo apt-get install libssl-dev\n"
            "  Fedora: sudo dnf install openssl-devel\n"
            "  Arch: sudo pacman -S openssl")
    endif()
elseif(APPLE)
    # macOS: Try to find OpenSSL (may be from Homebrew or system)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
        message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
        target_link_libraries(QNetworkRequest PRIVATE OpenSSL::SSL OpenSSL::Crypto)

        # Add Homebrew OpenSSL path if found via brew (both Intel and Apple Silicon)
        if(EXISTS "/opt/homebrew/opt/openssl")
            target_include_directories(QNetworkRequest PRIVATE "/opt/homebrew/opt/openssl/include")
            target_link_directories(QNetworkRequest PRIVATE "/opt/homebrew/opt/openssl/lib")
        elseif(EXISTS "/usr/local/opt/openssl")
            target_include_directories(QNetworkRequest PRIVATE "/usr/local/opt/openssl/include")
            target_link_directories(QNetworkRequest PRIVATE "/usr/local/opt/openssl/lib")
        endif()
    else()
        message(WARNING "OpenSSL not found on macOS. Install via Homebrew:")
        message(WARNING "  brew install openssl")
    endif()
endif()

# --- Build tests ---
enable_testing()
add_subdirectory(test)