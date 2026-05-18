#include "file_picker.h"

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

// Buffer must be large enough to hold the directory path plus all selected
// filenames (each up to MAX_PATH), separated by NULs. 32 KB covers ~50 files
// at MAX_PATH each, which is well above our 10-file UI cap.
#define MULTI_BUF_SIZE  (32 * 1024)

// Drive the common dialog in multi-select mode and stream each chosen path
// through `visit`. Returns the number of paths actually visited.
static int RunMultiSelectDialog(OPENFILENAMEA* ofn, FilePickCallback visit, void* user) {
    if (!GetOpenFileNameA(ofn)) return 0;     // cancel or error

    char* buf = ofn->lpstrFile;
    size_t firstLen = strlen(buf);
    char*  second   = buf + firstLen + 1;

    int count = 0;

    if (*second == '\0') {
        // Single-file path returned in `buf` as-is.
        if (visit && visit(buf, user)) count = 1;
        else if (!visit) count = 1;
        return count;
    }

    // Multi-select format: [directory]\0[file1]\0[file2]\0...\0\0
    char full[MAX_PATH];
    char* fn = second;
    while (*fn) {
        int n = snprintf(full, sizeof(full), "%s\\%s", buf, fn);
        if (n > 0 && n < (int)sizeof(full)) {
            count++;
            if (visit && !visit(full, user)) break;   // caller signalled stop
        }
        fn += strlen(fn) + 1;
    }
    return count;
}

int PickDocumentFiles(void* hwndOwner, FilePickCallback visit, void* user) {
    char buf[MULTI_BUF_SIZE];
    buf[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = (HWND)hwndOwner;
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = sizeof(buf);
    ofn.lpstrFilter =
        "Documents (*.txt;*.pdf)\0*.txt;*.pdf\0"
        "Text files (*.txt)\0*.txt\0"
        "PDF documents (*.pdf)\0*.pdf\0"
        "All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select one or more documents (Ctrl/Shift to multi-select)";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR
              | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    return RunMultiSelectDialog(&ofn, visit, user);
}

int PickImageFiles(void* hwndOwner, FilePickCallback visit, void* user) {
    char buf[MULTI_BUF_SIZE];
    buf[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = (HWND)hwndOwner;
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = sizeof(buf);
    ofn.lpstrFilter =
        "Images (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0"
        "All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select one or more images for OCR (Ctrl/Shift to multi-select)";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR
              | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    return RunMultiSelectDialog(&ofn, visit, user);
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
    ofn.nMaxFile  = outSize;
    ofn.lpstrFilter = "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrDefExt = "txt";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Save report";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    return GetSaveFileNameA(&ofn) ? true : false;
}
