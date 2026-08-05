@echo off
setlocal

REM ============================================================================
REM test.bat - Configure, build and run the QtMultiThreadNetwork test suite (Windows)
REM
REM This is the Windows equivalent of scripts/test_local.sh. It reuses the same
REM Qt / Visual Studio detection logic as scripts/build_win.bat.
REM
REM Usage:
REM   test.bat                 Configure + build (Release) + run ctest
REM   test.bat /clean         Remove <repo>\build first, then configure + build + test
REM   test.bat /nobuild       Skip the build step, run ctest against existing build
REM   test.bat /debug         Use Debug configuration instead of Release
REM   test.bat /help          Show this help
REM
REM Environment:
REM   QT_DIR  (or QTDIR)       Path to the Qt installation (e.g. C:\Qt\5.15.2\msvc2019_64)
REM ============================================================================

REM ---- Parse arguments ----
set CONFIG=Release
set DO_CLEAN=0
set SKIP_BUILD=0

if "%~1"=="/help" goto :show_help
if "%~1"=="-h" goto :show_help
if "%~1"=="--help" goto :show_help

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="/clean" ( set DO_CLEAN=1 & shift & goto :parse_args )
if /i "%~1"=="/nobuild" ( set SKIP_BUILD=1 & shift & goto :parse_args )
if /i "%~1"=="/debug" ( set CONFIG=Debug & shift & goto :parse_args )
echo Unknown argument: %~1
goto :show_help
:args_done

REM ---- Resolve the repository root by walking up from %CD% ----
REM %~dp0 / %~f0 are NOT used: when PowerShell invokes cmd.exe the %0
REM expansion is unreliable and always degrades to the drive root (e.g. "D:\").
REM Instead we search upward from the current working directory for
REM CMakeLists.txt, which is the repo-root marker.
set "REPO_ROOT=%CD%"
:find_cmake
if exist "%REPO_ROOT%\CMakeLists.txt" goto :found_repo
REM Go up one level (%%~fI normalizes ".." to absolute path)
for %%I in ("%REPO_ROOT%\..") do set "REPO_ROOT=%%~fI"
REM Stop if we hit a drive root (e.g. D:\) -- no CMakeLists.txt there either
for %%I in ("%REPO_ROOT%") do if /i "%%~dI\"=="%%~dpI" goto :cmake_not_found
goto :find_cmake
:cmake_not_found
echo Error: Could not find CMakeLists.txt by walking up from "%CD%"
echo        Please run this script from within the repository tree.
pause
exit /b 1

:found_repo
echo Running from: "%REPO_ROOT%"

REM ---- Check for Qt environment variables ----
if not defined QT_DIR (
    if not defined QTDIR (
        echo Error: Qt environment variable not set!
        echo Please set QT_DIR or QTDIR to your Qt installation.
        echo Example: set QT_DIR=C:\Qt\5.15.2\msvc2019_64
        pause
        exit /b 1
    ) else (
        set QT_DIR=%QTDIR%
    )
)

if not exist "%QT_DIR%" (
    echo Error: Qt directory not found: %QT_DIR%
    echo Please check your QT_DIR environment variable.
    pause
    exit /b 1
)

REM Add Qt bin directory to PATH
set PATH=%QT_DIR%\bin;%PATH%
echo Using Qt from: %QT_DIR%

REM ---- Detect Visual Studio version (prefer newest: 2022 > 2019 > 2017) ----
REM A goto-based cascade is used instead of a long "if/else if" chain: the chain
REM proved unreliable and fell through to the last (oldest) match. Here the first
REM existing install wins, so the newest available Visual Studio is selected.
echo Detecting Visual Studio...
set VS_VERSION=
set VS_GENERATOR=

if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" goto :vs2022
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" goto :vs2022
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" goto :vs2022
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" goto :vs2022
if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" goto :vs2019
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" goto :vs2019
if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" goto :vs2019
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" goto :vs2019
if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" goto :vs2017
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" goto :vs2017
if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" goto :vs2017
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" goto :vs2017
goto :vs_missing

:vs2022
set VS_VERSION=2022 & set "VS_GENERATOR=Visual Studio 17 2022" & echo Found Visual Studio 2022 & goto :vs_done
:vs2019
set VS_VERSION=2019 & set "VS_GENERATOR=Visual Studio 16 2019" & echo Found Visual Studio 2019 & goto :vs_done
:vs2017
set VS_VERSION=2017 & set "VS_GENERATOR=Visual Studio 15 2017" & echo Found Visual Studio 2017 & goto :vs_done

:vs_missing
echo Error: No Visual Studio installation found!
echo Please install Visual Studio 2017, 2019, or 2022.
pause
exit /b 1

:vs_done
echo Using Visual Studio %VS_VERSION% with generator "%VS_GENERATOR%"

REM ---- Optional clean ----
if %DO_CLEAN%==1 (
    if exist "%REPO_ROOT%\build" (
        echo Removing existing build directory...
        rmdir /s /q "%REPO_ROOT%\build"
    )
)

REM ---- Configure ----
echo Configuring project [config=%CONFIG%]...
cmake -S "%REPO_ROOT%" -B "%REPO_ROOT%\build" -G "%VS_GENERATOR%" -A x64 -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM ---- Build ----
if %SKIP_BUILD%==0 (
    echo Building project...
    cmake --build "%REPO_ROOT%\build" --config %CONFIG% --parallel
    if errorlevel 1 (
        echo Build failed!
        pause
        exit /b 1
    )
) else (
    echo Skipping build step [run ctest against existing build]...
)

REM ---- Run tests ----
echo Running tests with ctest...
pushd "%REPO_ROOT%\build"
ctest -C %CONFIG% --output-on-failure
set TEST_RESULT=%ERRORLEVEL%
popd

if %TEST_RESULT% neq 0 (
    echo.
    echo ============================================================
    echo  Some tests FAILED (ctest exit code: %TEST_RESULT%^)
    echo ============================================================
    pause
    exit /b %TEST_RESULT%
)

echo.
echo ============================================================
echo  All tests passed.
echo ============================================================
endlocal
exit /b 0

:show_help
echo.
echo test.bat - Configure, build and run the test suite [Windows]
echo.
echo Usage:
echo   test.bat                 Configure + build [Release] + run ctest
echo   test.bat /clean         Remove the build dir first, then configure + build + test
echo   test.bat /nobuild       Skip the build step, run ctest against existing build
echo   test.bat /debug         Use Debug configuration instead of Release
echo   test.bat /help          Show this help
echo.
echo Environment:
echo   QT_DIR  [or QTDIR]       Path to the Qt installation
echo.
endlocal
exit /b 0
