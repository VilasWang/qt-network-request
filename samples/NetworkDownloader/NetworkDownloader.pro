######################################################################
# Qt Downloader Project File
######################################################################

TEMPLATE = app
TARGET = QtNetworkDownloader

QT += core gui network widgets
CONFIG += debug_and_release c++17

# Include paths
INCLUDEPATH += . \
                $$PWD/../../include \
                $$PWD/resources

# Input files
SOURCES += \
    main.cpp \
    downloadermainwindow.cpp \
    downloadtaskmodel.cpp \
    downloadmanager.cpp

HEADERS += \
    downloadermainwindow.h \
    downloadtaskmodel.h \
    downloadmanager.h \
    downloadtask.h

FORMS += \
    NetworkDownloaderMainWindow.ui

# Architecture detection
greaterThan(QT_MAJOR_VERSION, 4) {
    TARGET_ARCH=$${QT_ARCH}
} else {
    TARGET_ARCH=$${QMAKE_HOST.arch}
}

# Output directories and library paths
CONFIG(debug, debug|release) {
        contains(TARGET_ARCH, x86_64) {
            DESTDIR = $$PWD/../../build/Debug
            LIBPATH += $$PWD/../../build/Debug
        } else {
            DESTDIR = $$PWD/../../build/Debug
            LIBPATH += $$PWD/../../build/Debug
        }
        LIBS += -lQNetworkRequestd
} else {
        contains(TARGET_ARCH, x86_64) {
            DESTDIR = $$PWD/../../build/Release
            LIBPATH += $$PWD/../../build/Release
        } else {
            DESTDIR = $$PWD/../../build/Release
            LIBPATH += $$PWD/../../build/Release
        }
        LIBS += -lQNetworkRequest
}

# Resources
RESOURCES += \
    NetworkDownloader.qrc

# Windows specific post-processing
win32 {
    # Copy OpenSSL DLL files to build directory
    greaterThan(QT_VERSION, 5.15.0) {
        OPENSSL_FILES = $$PWD/../../ThirdParty/openssl/1.1.1/bin/*.dll
    } else {
        OPENSSL_FILES = $$PWD/../../ThirdParty/openssl/1.0.2/bin/*.dll
    }
    OPENSSL_FILES = $$replace(OPENSSL_FILES, /, \\)
    OUTPUT_FILES_DIR = $$DESTDIR
    OUTPUT_FILES_DIR = $$replace(OUTPUT_FILES_DIR, /, \\)
    QMAKE_POST_LINK += copy /Y $$OPENSSL_FILES $$OUTPUT_FILES_DIR
}