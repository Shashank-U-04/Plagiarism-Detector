@echo off
setlocal

echo.
echo Building Plagiarism Detector (Win32 GUI)...
echo.

set SRC=main.c theme.c painter.c algorithm.c file_picker.c pdf_extract.c ocr_engine.c
set FLAGS=-Wall -Wextra -Wno-format-truncation -O2 -std=c99 -I"C:\msys64\mingw64\include" -I"C:\msys64\mingw64\include\tesseract" -L"C:\msys64\mingw64\lib"
set LIBS=-ltesseract -lgdi32 -lcomdlg32 -lcomctl32 -luser32 -lkernel32
set GUI=-mwindows

C:\msys64\mingw64\bin\gcc %SRC% %FLAGS% %GUI% -o plagiarism_detector.exe %LIBS%

if %ERRORLEVEL% EQU 0 (
    echo [OK] Build complete: plagiarism_detector.exe
) else (
    echo [ERROR] Build failed.
    echo Make sure gcc and tesseract are installed under C:\msys64\mingw64.
    exit /b 1
)

endlocal
