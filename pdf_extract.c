#include "pdf_extract.h"

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
