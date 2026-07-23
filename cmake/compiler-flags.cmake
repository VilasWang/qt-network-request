# ─── MSVC compiler/linker flags for all configurations ──────────────────────
# Include this after `project()` to apply platform-specific toolchain settings.

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Treat all source files as UTF-8 (suppresses C4819 on Chinese code page 936)
    add_compile_options("/utf-8")

    # Debug
    set(CMAKE_CXX_FLAGS_DEBUG          "/ZI /Od /MDd /RTC1 /GS /Ob0")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "/DEBUG /INCREMENTAL")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL")

    # Release
    set(CMAKE_CXX_FLAGS_RELEASE          "/O2 /MD")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE   "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF /OPT:ICF")

    # RelWithDebInfo
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO          "/O2 /Zi /MD")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO   "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")

    # MinSizeRel
    set(CMAKE_CXX_FLAGS_MINSIZEREL          "/O1 /MD")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL   "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
endif()
