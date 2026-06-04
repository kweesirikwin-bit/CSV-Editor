@echo off
REM Build script for CSV Viewer
REM This script tries multiple compilers to build a single portable .exe

echo ========================================
echo CSV Viewer Build Script
echo ========================================
echo.

REM Check for MinGW g++
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [*] Found g++ compiler - Building with MinGW...
    g++ csv_viewer.cpp -o csv_viewer.exe -mwindows -static -O2 -lcomctl32 -lcomdlg32 -lgdi32
    if %errorlevel% equ 0 (
        echo [+] Build successful with MinGW!
        echo [+] Output: csv_viewer.exe
        goto :success
    ) else (
        echo [-] Build failed with MinGW
    )
)

REM Check for MSVC cl
where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [*] Found cl compiler - Building with MSVC...
    cl /EHsc /O2 csv_viewer.cpp /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib comdlg32.lib /OUT:csv_viewer.exe
    if %errorlevel% equ 0 (
        echo [+] Build successful with MSVC!
        echo [+] Output: csv_viewer.exe
        del csv_viewer.obj >nul 2>nul
        goto :success
    ) else (
        echo [-] Build failed with MSVC
    )
)

REM Check for Clang
where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [*] Found clang++ compiler - Building with Clang...
    clang++ csv_viewer.cpp -o csv_viewer.exe -mwindows -static -O2 -lcomctl32 -lcomdlg32 -lgdi32
    if %errorlevel% equ 0 (
        echo [+] Build successful with Clang!
        echo [+] Output: csv_viewer.exe
        goto :success
    ) else (
        echo [-] Build failed with Clang
    )
)

echo.
echo [-] ERROR: No suitable C++ compiler found!
echo.
echo Please install one of the following:
echo   - MinGW-w64 (recommended): https://www.mingw-w64.org/
echo   - MSVC (Visual Studio): https://visualstudio.microsoft.com/
echo   - Clang for Windows: https://releases.llvm.org/
echo.
pause
exit /b 1

:success
echo.
echo ========================================
echo Build complete!
echo.
echo To run the program:
echo   csv_viewer.exe
echo.
echo Features:
echo   - Double-click window to open CSV file
echo   - Press Ctrl+O to open CSV file
echo   - Press ESC to exit
echo ========================================
pause
