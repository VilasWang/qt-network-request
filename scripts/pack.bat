@echo off
setlocal

REM Change to parent directory
cd ..
if errorlevel 1 (
    echo ERROR: Failed to change to parent directory
    pause
    exit /b 1
)

echo Starting packaging process...

REM 1) Create directory structure for harvesting
if not exist install (
    echo Creating install directory...
    mkdir install
    if errorlevel 1 (
        echo ERROR: Failed to create install directory
        pause
        exit /b 1
    )
) else (
    echo Install directory already exists, cleaning...
    rmdir /s /q install
    if errorlevel 1 (
        echo ERROR: Failed to clean install directory
        pause
        exit /b 1
    )
    mkdir install
    if errorlevel 1 (
        echo ERROR: Failed to recreate install directory
        pause
        exit /b 1
    )
)

if not exist installer (
    echo Creating installer directory...
    mkdir installer
    if errorlevel 1 (
        echo ERROR: Failed to create installer directory
        pause
        exit /b 1
    )
) else (
    echo installer directory already exists, cleaning...
    rmdir /s /q installer
    if errorlevel 1 (
        echo ERROR: Failed to clean installer directory
        pause
        exit /b 1
    )
    mkdir installer
    if errorlevel 1 (
        echo ERROR: Failed to recreate installer directory
        pause
        exit /b 1
    )
)

REM 2) Check if build directory exists
if not exist build\Release (
    echo ERROR: Build directory build\Release does not exist. Please build the project first.
    pause
    exit /b 1
)

REM 3) Copy files to install directory
echo Copying build files to install directory...
xcopy /E /I /Y build\Release\* install\ >nul
if errorlevel 1 (
    echo ERROR: Failed to copy build files to install directory
    pause
    exit /b 1
)

REM 5) Verify essential files exist
echo Verifying essential files...
if not exist install\QtRequester.exe (
    echo ERROR: QtRequester.exe not found in install directory
    pause
    exit /b 1
)
if not exist install\QtDownloader.exe (
    echo ERROR: QtDownloader.exe not found in install directory
    pause
    exit /b 1
)
if not exist install\QNetworkRequest.dll (
    echo ERROR: QNetworkRequest.dll not found in install directory
    pause
    exit /b 1
)

echo Files copied to install directory successfully.

rem Collect Qt runtime for QtRequester
echo Deploying Qt dependencies for QtRequester...
windeployqt --release --no-translations --no-system-d3d-compiler --compiler-runtime install\QtRequester.exe
if errorlevel 1 (
    echo windeployqt failed for QtRequester!
    pause
    exit /b 1
)

rem Collect Qt runtime for QtDownloader
echo Deploying Qt dependencies for QtDownloader...
windeployqt --release --no-translations --no-system-d3d-compiler --compiler-runtime install\QtDownloader.exe
if errorlevel 1 (
    echo windeployqt failed for QtDownloader!
    pause
    exit /b 1
)

REM 6) Determine Qt version and OpenSSL DLL requirements
echo Determining Qt version...
set USE_OPENSSL_11=yes
set OPENSSL_CRYPTO_DLL=libcrypto-1_1-x64.dll
set OPENSSL_SSL_DLL=libssl-1_1-x64.dll

REM Check if OpenSSL 1.0.2 DLLs exist (indicates Qt < 5.12)
if exist "install\libeay32.dll" (
    echo Found OpenSSL 1.0.2 DLLs - using legacy OpenSSL
    set USE_OPENSSL_11=no
    set OPENSSL_CRYPTO_DLL=libeay32.dll
    set OPENSSL_SSL_DLL=ssleay32.dll
) else (
    echo Found OpenSSL 1.1.1 DLLs - using modern OpenSSL
)

REM 7) Check if WiX toolset is available
echo Checking for WiX toolset...
wix --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: WiX toolset is not installed or not in PATH. Please install WiX toolset first.
    pause
    exit /b 1
)

REM 8) Build MSI packages using WiX v4 syntax with OpenSSL configuration
echo Building QtRequester MSI package with OpenSSL DLLs...
wix build scripts\Package.wxs -o installer\QtRequester-1.0.0.msi ^
    -d USE_OPENSSL_11=%USE_OPENSSL_11% ^
    -d OPENSSL_CRYPTO_DLL=%OPENSSL_CRYPTO_DLL% ^
    -d OPENSSL_SSL_DLL=%OPENSSL_SSL_DLL%
if errorlevel 1 (
    echo ERROR: Failed to build QtRequester MSI package
    pause
    exit /b 1
)

echo Building QtDownloader MSI package with OpenSSL DLLs...
wix build scripts\Package_QtDownloader.wxs -o installer\QtDownloader-1.0.0.msi ^
    -d USE_OPENSSL_11=%USE_OPENSSL_11% ^
    -d OPENSSL_CRYPTO_DLL=%OPENSSL_CRYPTO_DLL% ^
    -d OPENSSL_SSL_DLL=%OPENSSL_SSL_DLL%
if errorlevel 1 (
    echo ERROR: Failed to build QtDownloader MSI package
    pause
    exit /b 1
)

REM 9) Verify MSI packages were created
if not exist installer\QtRequester-1.0.0.msi (
    echo ERROR: QtRequester MSI package was not created
    pause
    exit /b 1
)

if not exist installer\QtDownloader-1.0.0.msi (
    echo ERROR: QtDownloader MSI package was not created
    pause
    exit /b 1
)

echo.
echo Packaging completed successfully!
echo MSI packages created:
echo - QtRequester-1.0.0.msi
echo - QtDownloader-1.0.0.msi

endlocal
exit /b 0
