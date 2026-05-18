#include "file_picker.h"

#include <windows.h>
#include <commdlg.h>
#include <string.h>

bool PickInputFile(char* outPath, int outSize) {
    if (!outPath || outSize <= 0) return false;
    outPath[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.lpstrFilter =
        "PDF documents (*.pdf)\0*.pdf\0"
        "Images (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0"
        "Text files (*.txt)\0*.txt\0"
        "All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select document to check for plagiarism";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    return GetOpenFileNameA(&ofn) ? true : false;
}

bool PickDocumentFile(void* hwndOwner, char* outPath, int outSize) {
    if (!outPath || outSize <= 0) return false;
    outPath[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.lpstrFilter =
        "Documents (*.txt;*.pdf)\0*.txt;*.pdf\0"
        "Text files (*.txt)\0*.txt\0"
        "PDF documents (*.pdf)\0*.pdf\0"
        "All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select a text or PDF document";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    return GetOpenFileNameA(&ofn) ? true : false;
}

bool PickImageFile(void* hwndOwner, char* outPath, int outSize) {
    if (!outPath || outSize <= 0) return false;
    outPath[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.lpstrFilter =
        "Images (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0"
        "All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select an image to OCR";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    return GetOpenFileNameA(&ofn) ? true : false;
}

bool PickSaveReportFile(void* hwndOwner, char* outPath, int outSize) {
    if (!outPath || outSize <= 0) return false;
    if (outPath[0] == '\0') {
        strncpy(outPath, "plagiarism_report.txt", outSize - 1);
        outPath[outSize - 1] = '\0';
    }

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)hwndOwner;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = outSize;
    ofn.lpstrFilter = "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrDefExt = "txt";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Save report";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    return GetSaveFileNameA(&ofn) ? true : false;
}
