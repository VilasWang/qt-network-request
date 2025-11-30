@echo off
echo Starting build and packaging process...
echo.

echo Step 1: Building the project...
call build_win.bat
if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)
echo Build completed successfully.
echo.

echo Step 2: Creating packages...
call pack.bat
if errorlevel 1 (
    echo ERROR: Packaging failed!
    pause
    exit /b 1
)
echo Packaging completed successfully.
echo.

echo All batch files executed successfully!
echo MSI packages are available in the installer directory.
pause