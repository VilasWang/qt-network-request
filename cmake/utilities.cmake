# ─── Shared utility functions ────────────────────────────────────────────────

# ---- set_output_directories --------------------------------------------------
# Standardizes per-config output for a target.
# Defaults to CMAKE_BINARY_DIR; override QTNETWORK_OUTPUT_ROOT for a custom root.
#
function(set_output_directories target)
    if(NOT DEFINED QTNETWORK_OUTPUT_ROOT)
        set(QTNETWORK_OUTPUT_ROOT "${CMAKE_BINARY_DIR}")
    endif()

    foreach(config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config}  "${QTNETWORK_OUTPUT_ROOT}/${config}"
            LIBRARY_OUTPUT_DIRECTORY_${config}  "${QTNETWORK_OUTPUT_ROOT}/${config}"
            ARCHIVE_OUTPUT_DIRECTORY_${config}  "${QTNETWORK_OUTPUT_ROOT}/${config}"
        )
    endforeach()
endfunction()

# ---- copy_openssl_dlls -------------------------------------------------------
# Copies the appropriate OpenSSL DLLs to a target's output directory on Windows.
#
function(copy_openssl_dlls target openssl_bin_dir)
    if(NOT WIN32)
        return()
    endif()

    # Qt6 always uses OpenSSL 1.1.x; Qt5 < 5.12 uses 1.0.x
    if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION_MINOR LESS 12)
        set(dlls libeay32.dll ssleay32.dll)
    else()
        set(dlls libcrypto-1_1-x64.dll libssl-1_1-x64.dll)
    endif()

    foreach(dll ${dlls})
        add_custom_command(
            TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${openssl_bin_dir}/${dll}"
            $<TARGET_FILE_DIR:${target}>
        )
    endforeach()
endfunction()

# ---- copy_library_artifact ---------------------------------------------------
# Copies a built library/shared object to a target's output directory (Windows).
# Useful so test executables find the DLL next to them.
#
function(copy_library_artifact target lib_target)
    if(NOT WIN32)
        return()
    endif()

    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:${lib_target}>/$<TARGET_FILE_NAME:${lib_target}>"
        $<TARGET_FILE_DIR:${target}>
    )
endfunction()
