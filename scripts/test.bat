@echo off
setlocal EnableDelayedExpansion

REM ============================================================================
REM test.bat - Configure, build and run the QtMultiThreadNetwork test suite (Windows)
REM
REM This is the Windows equivalent of scripts/test_local.sh. It reuses the same
REM Qt / Visual Studio detection logic as scripts/build_win.bat.
REM
REM Usage:
REM   test.bat                 Configure + build (Release) + run ctest
REM   test.bat /clean         Remove ..\build first, then configure + build + test
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

REM ---- Locate script directory and change into it ----
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"
echo Running from: "%SCRIPT_DIR%"

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

REM ---- Detect Visual Studio version ----
echo Detecting Visual Studio...
set VS_VERSION=
set VS_GENERATOR=

if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022 & set VS_GENERATOR="Visual Studio 17 2022" & echo Found Visual Studio 2022 Professional
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022 & set VS_GENERATOR="Visual Studio 17 2022" & echo Found Visual Studio 2022 Professional (x86)
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022 & set VS_GENERATOR="Visual Studio 17 2022" & echo Found Visual Studio 2022 Community
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022 & set VS_GENERATOR="Visual Studio 17 2022" & echo Found Visual Studio 2022 Community (x86)
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2019 & set VS_GENERATOR="Visual Studio 16 2019" & echo Found Visual Studio 2019 Professional
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2019 & set VS_GENERATOR="Visual Studio 16 2019" & echo Found Visual Studio 2019 Professional (x86)
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2019 & set VS_GENERATOR="Visual Studio 16 2019" & echo Found Visual Studio 2019 Community
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2019 & set VS_GENERATOR="Visual Studio 16 2019" & echo Found Visual Studio 2019 Community (x86)
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" (
    set VS_VERSION=2017 & set VS_GENERATOR="Visual Studio 15 2017" & echo Found Visual Studio 2017 Professional
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" (
    set VS_VERSION=2017 & set VS_GENERATOR="Visual Studio 15 2017" & echo Found Visual Studio 2017 Professional (x86)
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" (
    set VS_VERSION=2017 & set VS_GENERATOR="Visual Studio 15 2017" & echo Found Visual Studio 2017 Community
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" (
    set VS_VERSION=2017 & set VS_GENERATOR="Visual Studio 15 2017" & echo Found Visual Studio 2017 Community (x86)
)

if not defined VS_VERSION (
    echo Error: No Visual Studio installation found!
    echo Please install Visual Studio 2017, 2019, or 2022.
    pause
    exit /b 1
)
echo Using Visual Studio %VS_VERSION% with generator %VS_GENERATOR%

REM ---- Optional clean ----
if %DO_CLEAN%==1 (
    if exist ..\build (
        echo Removing existing build directory...
        rmdir /s /q ..\build
    )
)

REM ---- Configure ----
echo Configuring project (config=%CONFIG%)...
cmake -S .. -B ..\build -G %VS_GENERATOR% -A x64 -DCMAKE_BUILD_TYPE=%CONFIG%
if !errorlevel! neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM ---- Build ----
if %SKIP_BUILD%==0 (
    echo Building project...
    cmake --build ..\build --config %CONFIG% --parallel
    if !errorlevel! neq 0 (
        echo Build failed!
        pause
        exit /b 1
    )
) else (
    echo Skipping build step (run ctest against existing build)...
)

REM ---- Run tests ----
echo Running tests with ctest...
pushd ..\build
ctest -C %CONFIG% --output-on-failure
set TEST_RESULT=!errorlevel!
popd

if !TEST_RESULT! neq 0 (
    echo.
    echo ============================================================
    echo  Some tests FAILED (ctest exit code: !TEST_RESULT!^)
    echo ============================================================
    pause
    exit /b !TEST_RESULT!
)

echo.
echo ============================================================
echo  All tests passed.
echo ============================================================
endlocal
exit /b 0

:show_help
echo.
echo test.bat - Configure, build and run the test suite (Windows)
echo.
echo Usage:
echo   test.bat                 Configure + build (Release) + run ctest
echo   test.bat /clean         Remove ..\build first, then configure + build + test
echo   test.bat /nobuild       Skip the build step, run ctest against existing build
echo   test.bat /debug         Use Debug configuration instead of Release
echo   test.bat /help          Show this help
echo.
echo Environment:
echo   QT_DIR  (or QTDIR)       Path to the Qt installation
echo.
endlocal
exit /b 0
