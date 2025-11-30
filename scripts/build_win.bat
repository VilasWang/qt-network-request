@echo off
setlocal EnableDelayedExpansion

:: Check for Qt environment variables
if not defined QT_DIR (
    if not defined QTDIR (
        echo Error: Qt environment variable not set!
        echo Please set QT_DIR or QTDIR environment variable to point to your Qt installation.
        echo Example: set QT_DIR=C:\Qt\Qt5.6.3\5.6.3\msvc2015_64
        pause
        exit /b 1
    ) else (
        set QT_DIR=%QTDIR%
    )
)

:: Verify Qt directory exists
if not exist "%QT_DIR%" (
    echo Error: Qt directory not found: %QT_DIR%
    echo Please check your QT_DIR environment variable.
    pause
    exit /b 1
)

:: Add Qt bin directory to PATH
set PATH=%QT_DIR%\bin;%PATH%

echo Using Qt from: %QT_DIR%

:: Detect Visual Studio version
echo Detecting Visual Studio...
set VS_VERSION=
set VS_GENERATOR=

:: Check for VS 2022
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022
    set VS_GENERATOR="Visual Studio 17 2022"
    echo Found Visual Studio 2022 Professional
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022
    set VS_GENERATOR="Visual Studio 17 2022"
    echo Found Visual Studio 2022 Professional (x86)
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022
    set VS_GENERATOR="Visual Studio 17 2022"
    echo Found Visual Studio 2022 Community
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set VS_VERSION=2022
    set VS_GENERATOR="Visual Studio 17 2022"
    echo Found Visual Studio 2022 Community (x86)
)

:: Check for VS 2019
if not defined VS_VERSION (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set VS_VERSION=2019
        set VS_GENERATOR="Visual Studio 16 2019"
        echo Found Visual Studio 2019 Professional
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set VS_VERSION=2019
        set VS_GENERATOR="Visual Studio 16 2019"
        echo Found Visual Studio 2019 Professional (x86)
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set VS_VERSION=2019
        set VS_GENERATOR="Visual Studio 16 2019"
        echo Found Visual Studio 2019 Community
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set VS_VERSION=2019
        set VS_GENERATOR="Visual Studio 16 2019"
        echo Found Visual Studio 2019 Community (x86)
    )
)

:: Check for VS 2017
if not defined VS_VERSION (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" (
        set VS_VERSION=2017
        set VS_GENERATOR="Visual Studio 15 2017"
        echo Found Visual Studio 2017 Professional
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" (
        set VS_VERSION=2017
        set VS_GENERATOR="Visual Studio 15 2017"
        echo Found Visual Studio 2017 Professional (x86)
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" (
        set VS_VERSION=2017
        set VS_GENERATOR="Visual Studio 15 2017"
        echo Found Visual Studio 2017 Community
    ) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" (
        set VS_VERSION=2017
        set VS_GENERATOR="Visual Studio 15 2017"
        echo Found Visual Studio 2017 Community (x86)
    )
)

if not defined VS_VERSION (
    echo Error: No Visual Studio installation found!
    echo Please install Visual Studio 2017, 2019, or 2022.
    echo Or set VS_VERSION environment variable manually.
    pause
    exit /b 1
)

echo Using Visual Studio %VS_VERSION%

rem Clean old directories 
if exist ..\build rmdir /s /q ..\build
if exist ..\install rmdir /s /q ..\install

rem Configure project
echo Configuring project...
cmake -S .. -B ..\build -G %VS_GENERATOR% -A x64 -DCMAKE_BUILD_TYPE=Release
if !errorlevel! neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

rem Build project
echo Building project...
cmake --build ..\build --config Release --parallel
if !errorlevel! neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

rem === Run ctest if requested ===
if "%RUN_TESTS%"=="1" (
    echo Running tests with ctest...
    pushd ..\build\test\Release
    ctest --output-on-failure --build-config Release
    if !errorlevel! neq 0 (
        echo Some tests failed.
        popd
        pause
        exit /b 1
    )
    popd
)

echo Build completed successfully!
endlocal
pause