# ─── OpenSSL detection and linking (Unix: Linux / macOS) ─────────────────────
# Call after the library target exists.
#
macro(setup_openssl_unix target)
    if(UNIX AND NOT APPLE)
        # ── Linux ────────────────────────────────────────────────────────────
        find_package(OpenSSL)
        if(OPENSSL_FOUND)
            message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
            message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
            message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
            target_link_libraries(${target} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        else()
            message(FATAL_ERROR
                "OpenSSL not found. Install libssl-dev on Linux:\n"
                "  Ubuntu/Debian: sudo apt-get install libssl-dev\n"
                "  Fedora:         sudo dnf install openssl-devel\n"
                "  Arch:           sudo pacman -S openssl")
        endif()

    elseif(APPLE)
        # ── macOS ────────────────────────────────────────────────────────────
        find_package(OpenSSL)
        if(OPENSSL_FOUND)
            message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
            message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
            target_link_libraries(${target} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

            if(EXISTS "/opt/homebrew/opt/openssl")
                target_include_directories(${target} PRIVATE "/opt/homebrew/opt/openssl/include")
                target_link_directories(${target} PRIVATE "/opt/homebrew/opt/openssl/lib")
            elseif(EXISTS "/usr/local/opt/openssl")
                target_include_directories(${target} PRIVATE "/usr/local/opt/openssl/include")
                target_link_directories(${target} PRIVATE "/usr/local/opt/openssl/lib")
            endif()
        else()
            message(WARNING "OpenSSL not found on macOS. Install via Homebrew:")
            message(WARNING "  brew install openssl")
        endif()
    endif()
endmacro()
