TEMPLATE = app
TARGET = UnitTests
CONFIG += console
CONFIG -= app_bundle

QT += core network testlib

# Add test source files
SOURCES += \
    main.cpp \
    test_networkrequest.cpp

HEADERS += \
    test_networkrequest.h

# Include network request library header file paths
INCLUDEPATH += ../include ../source

CONFIG(debug, debug|release) {
        contains(TARGET_ARCH, x86_64) {
            DESTDIR = $$PWD/../build/Debug
            LIBPATH += $$PWD/../build/Debug
        } else {
            DESTDIR = $$PWD/../build/Debug
            LIBPATH += $$PWD/../build/Debug
        }
        LIBS += -lQNetworkRequestd
} else {
        contains(TARGET_ARCH, x86_64) {
            DESTDIR = $$PWD/../build/Release
            LIBPATH += $$PWD/../build/Release
        } else {
            DESTDIR = $$PWD/../build/Release
            LIBPATH += $$PWD/../build/Release
        }
        LIBS += -lQNetworkRequest
}

# Windows specific post-processing
win32 {
    # Copy OpenSSL DLL files to build directory
    # Debug Qt version
message(Qt version: $$QT_VERSION)
message(Qt major.minor: $$QT_MAJOR_VERSION.$$QT_MINOR_VERSION)

# Check Qt version correctly
equals(QT_MAJOR_VERSION, 5) {
    lessThan(QT_MINOR_VERSION, 15) {
        OPENSSL_FILES = $$PWD/../ThirdParty/openssl/1.0.2/bin/*.dll
        message(Using OpenSSL 1.0.2 for Qt < 5.15)
    } else {
        OPENSSL_FILES = $$PWD/../ThirdParty/openssl/1.1.1/bin/*.dll
        message(Using OpenSSL 1.1.1 for Qt >= 5.15)
    }
} else {
    # Qt 6+ should use OpenSSL 1.1.1
    OPENSSL_FILES = $$PWD/../ThirdParty/openssl/1.1.1/bin/*.dll
    message(Using OpenSSL 1.1.1 for Qt 6+)
}
    OPENSSL_FILES = $$replace(OPENSSL_FILES, /, \\)
    OUTPUT_FILES_DIR = $$DESTDIR
    OUTPUT_FILES_DIR = $$replace(OUTPUT_FILES_DIR, /, \\)
    QMAKE_POST_LINK += copy /Y $$OPENSSL_FILES $$OUTPUT_FILES_DIR
}