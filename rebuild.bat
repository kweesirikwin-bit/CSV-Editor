@echo off
echo Rebuilding CSV Viewer...
echo.
g++ csv_viewer.cpp -o csv_viewer.exe -mwindows -static -O2 -lcomctl32 -lcomdlg32 -lgdi32
if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo Build Successful!
    echo ========================================
    echo.
    echo csv_viewer.exe has been created.
    echo You can now run it!
    echo.
    echo Press any key to launch the CSV Viewer...
    pause >nul
    start csv_viewer.exe
) else (
    echo.
    echo Build failed!
    pause
)
