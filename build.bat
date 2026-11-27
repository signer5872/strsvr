@echo off
REM ============================================================
REM STRSVR Linux Port - Windows Build Script
REM ============================================================
REM Build STRSVR on Windows with Qt and MSVC/MinGW.
REM
REM Prerequisites:
REM   - Qt 5.15+ with MinGW or MSVC
REM   - CMake 3.13+
REM   - Visual Studio 2019+ OR MinGW-w64 with g++
REM ============================================================

setlocal

echo ==============================================
echo  STRSVR - GNSS Stream Server (Windows Build)
echo ==============================================

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo --- Configuring build ---
cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

echo.
echo --- Building ---
cmake --build "%BUILD_DIR%" --config Release

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ==============================================
echo  Build complete!
echo ==============================================

endlocal
