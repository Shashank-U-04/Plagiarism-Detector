#include "pdf_extract.h"
#include "ocr_engine.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POPPLER_BIN "C:\\msys64\\mingw64\\bin"

// Spawn a process and capture its stdout into a malloc'd buffer.
// Returns NULL on failure or non-zero exit. Buffer is null-terminated.
static char* RunAndCaptureStdout(const char* cmdLine) {
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hRead = NULL, hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return NULL;
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return NULL;
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char* cmdMut = _strdup(cmdLine);
    if (!cmdMut) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return NULL;
    }

    BOOL ok = CreateProcessA(NULL, cmdMut, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmdMut);
    CloseHandle(hWrite);
    if (!ok) {
        CloseHandle(hRead);
        return NULL;
    }

    size_t cap = 8192, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) {
        CloseHandle(hRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return NULL;
    }

    DWORD nRead = 0;
    char chunk[4096];
    while (ReadFile(hRead, chunk, (DWORD)sizeof(chunk), &nRead, NULL) && nRead > 0) {
        if (len + nRead + 1 > cap) {
            while (len + nRead + 1 > cap) cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) {
                free(buf);
                CloseHandle(hRead);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return NULL;
            }
            buf = nb;
        }
        memcpy(buf + len, chunk, nRead);
        len += nRead;
    }
    buf[len] = '\0';

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0 || len == 0) {
        free(buf);
        return NULL;
    }
    return buf;
}

// Run a command, discard stdout/stderr, return exit code. Used for pdftoppm
// which writes its output to disk, not stdout.
static int RunAndWait(const char* cmdLine) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char* cmdMut = _strdup(cmdLine);
    if (!cmdMut) return -1;

    BOOL ok = CreateProcessA(NULL, cmdMut, NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmdMut);
    if (!ok) return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
}

char* ExtractTextFromPDF(const char* pdfPath) {
    if (!pdfPath || !*pdfPath) return NULL;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"" POPPLER_BIN "\\pdftotext.exe\" -layout \"%s\" -",
             pdfPath);
    return RunAndCaptureStdout(cmd);
}

int GetPDFPageCount(const char* pdfPath) {
    if (!pdfPath || !*pdfPath) return 0;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"" POPPLER_BIN "\\pdfinfo.exe\" \"%s\"",
             pdfPath);
    char* out = RunAndCaptureStdout(cmd);
    if (!out) return 0;

    int pages = 0;
    const char* p = strstr(out, "Pages:");
    if (p) sscanf(p, "Pages: %d", &pages);
    free(out);
    return pages;
}

// ---------- handwritten-PDF OCR pipeline ----------

// Delete every file in `dir`, then remove the dir itself. Best-effort.
static void DeleteTempDir(const char* dir) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;
            char full[MAX_PATH];
            snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);
            DeleteFileA(full);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

// Build a unique temp directory under %TEMP%. On success writes the path
// to `out` and returns 1; returns 0 on failure.
static int MakeTempDir(char* out, int outSize) {
    char tempBase[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, tempBase);
    if (n == 0 || n >= MAX_PATH) return 0;

    DWORD pid  = GetCurrentProcessId();
    DWORD tick = GetTickCount();
    snprintf(out, outSize, "%splagi_%lu_%lu", tempBase,
             (unsigned long)pid, (unsigned long)tick);

    if (!CreateDirectoryA(out, NULL)) return 0;
    return 1;
}

// Comparator for qsort over an array of MAX_PATH-sized filename buffers.
static int CmpPageName(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

// Enumerate PNG pages in `dir`, sort lexicographically (pdftoppm pads its
// page numbers consistently within one job, so lex order == page order),
// fill `names` with up to `cap` entries, return count.
static int EnumeratePages(const char* dir, char (*names)[MAX_PATH], int cap) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.png", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    int count = 0;
    do {
        if (count >= cap) break;
        snprintf(names[count], MAX_PATH, "%s", fd.cFileName);
        count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    qsort(names, count, MAX_PATH, CmpPageName);
    return count;
}

char* ExtractTextFromHandwrittenPDF(const char* pdfPath,
                                    PdfOcrProgressFn progress,
                                    void* user) {
    if (!pdfPath || !*pdfPath) return NULL;

    char tempDir[MAX_PATH];
    if (!MakeTempDir(tempDir, sizeof(tempDir))) return NULL;

    // pdftoppm prefix -> tempDir\p
    char prefix[MAX_PATH];
    snprintf(prefix, sizeof(prefix), "%s\\p", tempDir);

    // 200 DPI PNG -- standard sweet spot for handwriting OCR
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"" POPPLER_BIN "\\pdftoppm.exe\" -r 200 -png \"%s\" \"%s\"",
             pdfPath, prefix);

    int rc = RunAndWait(cmd);
    if (rc != 0) {
        DeleteTempDir(tempDir);
        return NULL;
    }

    enum { MAX_PAGES = 1024 };
    char (*names)[MAX_PATH] = (char(*)[MAX_PATH])malloc(MAX_PAGES * MAX_PATH);
    if (!names) {
        DeleteTempDir(tempDir);
        return NULL;
    }

    int pageCount = EnumeratePages(tempDir, names, MAX_PAGES);
    if (pageCount == 0) {
        free(names);
        DeleteTempDir(tempDir);
        return NULL;
    }

    size_t cap = 16384, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) {
        free(names);
        DeleteTempDir(tempDir);
        return NULL;
    }
    out[0] = '\0';

    for (int i = 0; i < pageCount; i++) {
        if (progress) progress(i + 1, pageCount, user);

        char imgPath[MAX_PATH];
        snprintf(imgPath, sizeof(imgPath), "%s\\%s", tempDir, names[i]);

        char* pageText = ExtractTextFromImage(imgPath);
        if (!pageText) continue;   // skip unreadable page, don't abort whole doc

        size_t pageLen = strlen(pageText);
        size_t need = len + pageLen + 3;   // +"\n\n" + NUL
        if (need > cap) {
            while (need > cap) cap *= 2;
            char* nb = (char*)realloc(out, cap);
            if (!nb) {
                free(pageText);
                free(out);
                free(names);
                DeleteTempDir(tempDir);
                return NULL;
            }
            out = nb;
        }

        if (len > 0) {
            out[len++] = '\n';
            out[len++] = '\n';
        }
        memcpy(out + len, pageText, pageLen);
        len += pageLen;
        out[len] = '\0';

        free(pageText);
    }

    free(names);
    DeleteTempDir(tempDir);

    if (len == 0) {
        free(out);
        return NULL;
    }
    return out;
}
