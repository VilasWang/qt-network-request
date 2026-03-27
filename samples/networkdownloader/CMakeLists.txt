cmake_minimum_required(VERSION 3.15)
project(QtDownloader)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

# Source files
set(DOWNLOADER_SOURCES
    main.cpp
    downloadermainwindow.cpp
    downloadtaskmodel.cpp
    downloadmanager.cpp
)

# Header files
set(DOWNLOADER_HEADERS
    downloadermainwindow.h
    downloadtaskmodel.h
    downloadmanager.h
    downloadtask.h
)

# Form files
set(DOWNLOADER_FORMS
    downloadermainwindow.ui
)

# Add executable
add_executable(QtDownloader
    ${DOWNLOADER_SOURCES}
    ${DOWNLOADER_HEADERS}
    ${DOWNLOADER_FORMS}
)

# Include directories
target_include_directories(QtDownloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Link Qt libraries
target_link_libraries(QtDownloader PRIVATE
    QNetworkRequest
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)

# Windows specific settings
if(WIN32)
    target_compile_definitions(QtDownloader PRIVATE UNICODE _UNICODE)
    set_property(TARGET QtDownloader PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # Set output directories to match networkrequesttool
    set_target_properties(QtDownloader PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Release"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Debug"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Release"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Debug"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Release"
    )
endif()

# Install rules
install(TARGETS QtDownloader
    RUNTIME DESTINATION bin
)