@echo off
setlocal

set PATH=C:\msys64\mingw64\bin;%PATH%
set TESSDATA_PREFIX=C:\msys64\mingw64\share\tessdata

if not exist plagiarism_detector.exe (
    echo [ERROR] plagiarism_detector.exe not found. Run build.bat first.
    pause
    exit /b 1
)

start "" plagiarism_detector.exe

endlocal
