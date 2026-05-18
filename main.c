#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "theme.h"
#include "painter.h"
#include "file_picker.h"
#include "algorithm.h"
#include "pdf_extract.h"
#include "ocr_engine.h"

#define APP_TITLE   "Plagiarism Detector"
#define APP_CLASS   "PlagiarismDetectorMainWnd"

#define WIN_W       1180
#define WIN_H       820

#define MAX_DOCS    10

// Control IDs
#define ID_BTN_ADD_TEXT  101
#define ID_BTN_ADD_OCR   102
#define ID_BTN_ANALYZE   105
#define ID_BTN_SAVE      106
#define ID_BTN_RESET     107
#define ID_STATUS        110

typedef struct {
    char  path[MAX_PATH];
    char  name[256];
    char* text;        // raw extracted text (malloc'd)
    char* normalized;  // normalized text (malloc'd)
    int   is_ocr;      // 1 if loaded via OCR
} DocState;

typedef struct {
    HWND hwndMain;
    HWND hwndStatus;
    HWND hwndBtnAddText;
    HWND hwndBtnAddOcr;
    HWND hwndBtnAnalyze;
    HWND hwndBtnSave;
    HWND hwndBtnReset;

    DocState docs[MAX_DOCS];
    int      docCount;

    // Per-row remove-button hit rects, written by the painter and consumed by
    // WM_LBUTTONDOWN. Length tracks docCount each paint.
    RECT     removeRects[MAX_DOCS];

    // Matrix state
    double   matrix[MAX_DOCS * MAX_DOCS];
    int      matrixCount;             // 0 when no analysis is current
    int      top_a;
    int      top_b;
    double   top_percentage;
    int      top_lcs_length;
    char*    top_lcs_preview;         // owned
    char     top_verdict_buf[64];
    COLORREF top_verdict_color;

    // Cached pointer arrays passed to painter (kept here so they live long enough)
    const char* nameSlots[MAX_DOCS];

    // Layout rects
    RECT listRect;
    RECT resultsRect;
    RECT statusRect;

    BOOL ocrInited;
    BOOL ocrAvailable;
} App;

static App g_app;

// ---------------------------------------------------------------------------
// utility helpers
// ---------------------------------------------------------------------------

static const char* GetBaseName(const char* p) {
    const char* a = strrchr(p, '\\');
    const char* b = strrchr(p, '/');
    const char* s = a > b ? a : b;
    return s ? s + 1 : p;
}

static const char* GetExtLower(const char* path) {
    static char ext[16];
    const char* dot = strrchr(path, '.');
    if (!dot) { ext[0] = '\0'; return ext; }
    int i = 0;
    for (; dot[i] && i < (int)sizeof(ext) - 1; i++)
        ext[i] = (char)tolower((unsigned char)dot[i]);
    ext[i] = '\0';
    return ext;
}

static char* ReadFileText(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void DocReset(DocState* d) {
    free(d->text); d->text = NULL;
    free(d->normalized); d->normalized = NULL;
    d->path[0] = '\0';
    d->name[0] = '\0';
    d->is_ocr = 0;
}

static void MatrixReset(App* a) {
    free(a->top_lcs_preview); a->top_lcs_preview = NULL;
    a->matrixCount = 0;
    a->top_a = a->top_b = 0;
    a->top_percentage = 0.0;
    a->top_lcs_length = 0;
    a->top_verdict_buf[0] = '\0';
    a->top_verdict_color = CLR_TEXT_PRIMARY;
}

static void SetStatus(App* a, const char* msg) {
    if (a->hwndStatus) SetWindowTextA(a->hwndStatus, msg);
}

static const char* VerdictFor(double pct, COLORREF* color) {
    if (pct >= 70.0)      { *color = CLR_DANGER;  return "HIGH - Likely Plagiarism"; }
    else if (pct >= 40.0) { *color = CLR_WARNING; return "MEDIUM - Suspicious"; }
    else                  { *color = CLR_SUCCESS; return "LOW - Looks Original"; }
}

// ---------------------------------------------------------------------------
// document loading / removal
// ---------------------------------------------------------------------------

static int LoadDocFromPath(App* a, DocState* d, const char* path, BOOL useOcr) {
    DocReset(d);
    strncpy(d->path, path, sizeof(d->path) - 1);
    d->path[sizeof(d->path) - 1] = '\0';
    strncpy(d->name, GetBaseName(path), sizeof(d->name) - 1);
    d->name[sizeof(d->name) - 1] = '\0';

    const char* ext = GetExtLower(path);
    char* text = NULL;

    if (useOcr || strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 ||
        strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".bmp") == 0 ||
        strcmp(ext, ".tif") == 0 || strcmp(ext, ".tiff") == 0) {
        if (!a->ocrInited) {
            a->ocrAvailable = InitializeOCR();
            a->ocrInited = TRUE;
        }
        if (!a->ocrAvailable) {
            SetStatus(a, "OCR unavailable - check tesseract install and TESSDATA_PREFIX");
            return 0;
        }
        SetStatus(a, "Running OCR ...");
        text = ExtractTextFromImage(path);
        d->is_ocr = 1;
    } else if (strcmp(ext, ".pdf") == 0) {
        SetStatus(a, "Extracting PDF text ...");
        text = ExtractTextFromPDF(path);
    } else {
        SetStatus(a, "Reading file ...");
        text = ReadFileText(path);
    }

    if (!text) {
        SetStatus(a, "Failed to extract text from file");
        return 0;
    }

    d->text = text;
    d->normalized = (char*)malloc(strlen(text) + 1);
    if (d->normalized) NormalizeText(d->normalized, text);
    return 1;
}

static void HandleAdd(App* a, BOOL useOcr) {
    if (a->docCount >= MAX_DOCS) {
        SetStatus(a, "Maximum of 10 documents reached");
        return;
    }
    char path[MAX_PATH] = "";
    BOOL ok = useOcr
        ? PickImageFile(a->hwndMain, path, sizeof(path))
        : PickDocumentFile(a->hwndMain, path, sizeof(path));
    if (!ok) return;

    DocState* d = &a->docs[a->docCount];
    if (LoadDocFromPath(a, d, path, useOcr)) {
        a->docCount++;
        MatrixReset(a);                // any prior analysis is stale
        EnableWindow(a->hwndBtnSave, FALSE);
        EnableWindow(a->hwndBtnAnalyze, a->docCount >= 2);
        EnableWindow(a->hwndBtnAddText, a->docCount < MAX_DOCS);
        EnableWindow(a->hwndBtnAddOcr,  a->docCount < MAX_DOCS);
        InvalidateRect(a->hwndMain, NULL, TRUE);
        char msg[512];
        snprintf(msg, sizeof(msg), "Loaded %s (%zu chars). Total: %d/%d",
                 d->name, d->normalized ? strlen(d->normalized) : 0,
                 a->docCount, MAX_DOCS);
        SetStatus(a, msg);
    }
}

static void RemoveDoc(App* a, int idx) {
    if (idx < 0 || idx >= a->docCount) return;
    DocReset(&a->docs[idx]);
    for (int i = idx; i < a->docCount - 1; i++) {
        a->docs[i] = a->docs[i + 1];
    }
    memset(&a->docs[a->docCount - 1], 0, sizeof(DocState));
    a->docCount--;

    MatrixReset(a);
    EnableWindow(a->hwndBtnSave, FALSE);
    EnableWindow(a->hwndBtnAnalyze, a->docCount >= 2);
    EnableWindow(a->hwndBtnAddText, a->docCount < MAX_DOCS);
    EnableWindow(a->hwndBtnAddOcr,  a->docCount < MAX_DOCS);
    InvalidateRect(a->hwndMain, NULL, TRUE);

    char msg[64];
    snprintf(msg, sizeof(msg), "Removed file. Total: %d/%d", a->docCount, MAX_DOCS);
    SetStatus(a, msg);
}

// ---------------------------------------------------------------------------
// analysis (N-way matrix)
// ---------------------------------------------------------------------------

static void RunAnalysis(App* a) {
    if (a->docCount < 2) {
        SetStatus(a, "Add at least 2 documents before analyzing");
        return;
    }
    SetStatus(a, "Analyzing pairs ...");

    MatrixReset(a);
    a->matrixCount = a->docCount;
    int n = a->matrixCount;

    // Fill matrix with rolling-row LCS (length-only, cheap memory)
    for (int i = 0; i < n; i++) {
        a->matrix[i * n + i] = 0.0;
        for (int j = i + 1; j < n; j++) {
            const char* t1 = a->docs[i].normalized ? a->docs[i].normalized : "";
            const char* t2 = a->docs[j].normalized ? a->docs[j].normalized : "";
            double pct = CalculateSimilarity(t1, t2);
            a->matrix[i * n + j] = pct;
            a->matrix[j * n + i] = pct;
        }
    }

    // Pick the highest pair
    double best = -1.0;
    int ba = 0, bb = 1;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double v = a->matrix[i * n + j];
            if (v > best) { best = v; ba = i; bb = j; }
        }
    }
    a->top_a = ba;
    a->top_b = bb;
    a->top_percentage = best < 0 ? 0.0 : best;

    // Recover matched segment for the top pair via full-DP
    int lcs_len = 0;
    char* lcs = NULL;
    const char* tA = a->docs[ba].normalized ? a->docs[ba].normalized : "";
    const char* tB = a->docs[bb].normalized ? a->docs[bb].normalized : "";
    double pctVerify = AnalyzeLCS(tA, tB, &lcs_len, &lcs);
    (void)pctVerify;  // matches the rolling-row figure modulo rounding
    a->top_lcs_length = lcs_len;
    a->top_lcs_preview = lcs;

    COLORREF vc;
    const char* v = VerdictFor(a->top_percentage, &vc);
    strncpy(a->top_verdict_buf, v, sizeof(a->top_verdict_buf) - 1);
    a->top_verdict_buf[sizeof(a->top_verdict_buf) - 1] = '\0';
    a->top_verdict_color = vc;

    EnableWindow(a->hwndBtnSave, TRUE);
    InvalidateRect(a->hwndMain, &a->resultsRect, TRUE);

    char msg[128];
    snprintf(msg, sizeof(msg),
             "Analysis complete: %d pairs, highest %.1f%%",
             n * (n - 1) / 2, a->top_percentage);
    SetStatus(a, msg);
}

// ---------------------------------------------------------------------------
// save report (with ASCII matrix)
// ---------------------------------------------------------------------------

static void WriteReportMatrix(FILE* f, App* a) {
    int n = a->matrixCount;
    if (n <= 0) return;

    fprintf(f, "SIMILARITY MATRIX (%%)\r\n");

    // Header row: column numbers
    fprintf(f, "         ");
    for (int j = 0; j < n; j++) fprintf(f, "  #%-5d", j + 1);
    fprintf(f, "\r\n");

    // Data rows
    for (int i = 0; i < n; i++) {
        fprintf(f, "  #%-3d   ", i + 1);
        for (int j = 0; j < n; j++) {
            if (i == j) fprintf(f, "  %-5s ", "-");
            else        fprintf(f, "  %5.1f ", a->matrix[i * n + j]);
        }
        fprintf(f, "  %s\r\n", a->docs[i].name);
    }
    fprintf(f, "\r\n");

    fprintf(f, "FILE INDEX\r\n");
    for (int i = 0; i < n; i++) {
        fprintf(f, "  #%-2d  %s%s\r\n", i + 1,
                a->docs[i].name,
                a->docs[i].is_ocr ? "   [OCR]" : "");
        fprintf(f, "        %s\r\n", a->docs[i].path);
    }
    fprintf(f, "\r\n");
}

static void SaveReport(App* a) {
    if (a->matrixCount <= 0) {
        SetStatus(a, "Nothing to save - run analysis first");
        return;
    }
    char path[MAX_PATH] = "plagiarism_report.txt";
    if (!PickSaveReportFile(a->hwndMain, path, (int)sizeof(path))) return;

    FILE* f = fopen(path, "wb");
    if (!f) { SetStatus(a, "Could not open file for writing"); return; }

    time_t now = time(NULL);
    struct tm* lt = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);

    int n = a->matrixCount;
    int pairCount = n * (n - 1) / 2;

    fprintf(f, "PLAGIARISM DETECTION REPORT\r\n");
    fprintf(f, "Generated : %s\r\n", ts);
    fprintf(f, "Files     : %d\r\n", n);
    fprintf(f, "Pairs     : %d\r\n", pairCount);
    fprintf(f, "----------------------------------------\r\n");
    fprintf(f, "Highest pair : %s  vs  %s\r\n",
            a->docs[a->top_a].name, a->docs[a->top_b].name);
    fprintf(f, "Similarity   : %.2f %%\r\n", a->top_percentage);
    fprintf(f, "Verdict      : %s\r\n", a->top_verdict_buf);
    fprintf(f, "LCS length   : %d characters\r\n", a->top_lcs_length);
    fprintf(f, "----------------------------------------\r\n");

    WriteReportMatrix(f, a);

    fprintf(f, "----------------------------------------\r\n");
    fprintf(f, "Matching LCS preview (top pair):\r\n");
    if (a->top_lcs_preview) fputs(a->top_lcs_preview, f);
    fprintf(f, "\r\n");
    fclose(f);
    SetStatus(a, "Report saved");
}

static void ResetAll(App* a) {
    for (int i = 0; i < a->docCount; i++) DocReset(&a->docs[i]);
    a->docCount = 0;
    MatrixReset(a);
    EnableWindow(a->hwndBtnSave, FALSE);
    EnableWindow(a->hwndBtnAnalyze, FALSE);
    EnableWindow(a->hwndBtnAddText, TRUE);
    EnableWindow(a->hwndBtnAddOcr,  TRUE);
    InvalidateRect(a->hwndMain, NULL, TRUE);
    SetStatus(a, "Reset");
}

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------

static void Layout(App* a) {
    RECT cr;
    GetClientRect(a->hwndMain, &cr);
    int W = cr.right - cr.left;
    int H = cr.bottom - cr.top;

    int margin = 16;
    int titleH = 56;

    // File list panel: enough to show MAX_DOCS rows
    int listH = 32 + MAX_DOCS * 24 + 16;     // header + rows + padding
    a->listRect.left   = margin;
    a->listRect.top    = titleH;
    a->listRect.right  = W - margin;
    a->listRect.bottom = titleH + listH;

    // Add buttons row directly under the file list
    int btnRowY = a->listRect.bottom + 10;
    int addW = 180;
    int addGap = 12;
    int addH = 32;
    MoveWindow(a->hwndBtnAddText, margin,                   btnRowY, addW, addH, TRUE);
    MoveWindow(a->hwndBtnAddOcr,  margin + addW + addGap,   btnRowY, addW, addH, TRUE);

    // Analyze button (centered, same row, right of add buttons)
    int anaW = 240;
    int anaH = 38;
    MoveWindow(a->hwndBtnAnalyze, W - margin - anaW, btnRowY - 3, anaW, anaH, TRUE);

    // Results panel
    int resY = btnRowY + addH + 14;
    int resH = H - resY - margin - 56;
    if (resH < 200) resH = 200;
    a->resultsRect.left   = margin;
    a->resultsRect.top    = resY;
    a->resultsRect.right  = W - margin;
    a->resultsRect.bottom = resY + resH;

    // Bottom toolbar
    int botY = a->resultsRect.bottom + 10;
    int saveW = 140;
    MoveWindow(a->hwndBtnSave,  margin,             botY, saveW, 32, TRUE);
    MoveWindow(a->hwndBtnReset, margin + saveW + 8, botY, saveW, 32, TRUE);

    a->statusRect.left   = margin + (saveW + 8) * 2;
    a->statusRect.top    = botY;
    a->statusRect.right  = W - margin;
    a->statusRect.bottom = botY + 32;
    MoveWindow(a->hwndStatus,
               a->statusRect.left, a->statusRect.top + 6,
               a->statusRect.right - a->statusRect.left, 24, TRUE);
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------

static void OnPaint(App* a, HDC hdc) {
    RECT cr;
    GetClientRect(a->hwndMain, &cr);

    HBRUSH bg = CreateSolidBrush(CLR_BACKGROUND);
    FillRect(hdc, &cr, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);

    // Title bar text
    HFONT old = (HFONT)SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    TextOutA(hdc, 16, 12, "PLAGIARISM DETECTOR", 19);

    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    const char* sub = "Compare up to 10 documents using LCS dynamic programming";
    TextOutA(hdc, 16, 40, sub, (int)strlen(sub));

    // File list
    FileListRow rows[MAX_DOCS];
    for (int i = 0; i < a->docCount; i++) {
        rows[i].index      = i + 1;
        rows[i].name       = a->docs[i].name;
        rows[i].is_ocr     = a->docs[i].is_ocr;
        rows[i].char_count = a->docs[i].normalized ? (int)strlen(a->docs[i].normalized) : 0;
        memset(&rows[i].removeRect, 0, sizeof(RECT));
    }
    DrawFileListPanel(hdc, a->listRect, rows, a->docCount, MAX_DOCS,
                      a->docCount < MAX_DOCS);
    for (int i = 0; i < a->docCount; i++) a->removeRects[i] = rows[i].removeRect;

    // Results
    MatrixResult mr;
    memset(&mr, 0, sizeof(mr));
    if (a->matrixCount > 0) {
        for (int i = 0; i < a->matrixCount; i++) a->nameSlots[i] = a->docs[i].name;
        mr.count             = a->matrixCount;
        mr.names             = a->nameSlots;
        mr.matrix            = a->matrix;
        mr.top_a             = a->top_a;
        mr.top_b             = a->top_b;
        mr.top_percentage    = a->top_percentage;
        mr.top_lcs_length    = a->top_lcs_length;
        mr.top_verdict       = a->top_verdict_buf;
        mr.top_verdict_color = a->top_verdict_color;
        mr.top_lcs_preview   = a->top_lcs_preview;
        mr.has_result        = 1;
    }
    DrawMatrixResultsPanel(hdc, a->resultsRect, &mr);

    SelectObject(hdc, old);
}

// ---------------------------------------------------------------------------
// WndProc plumbing
// ---------------------------------------------------------------------------

static HWND MakeOwnerButton(HWND parent, int id, int x, int y, int w, int h, const char* text) {
    HWND b = CreateWindowExA(0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(LONG_PTR)id,
        (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE), NULL);
    SendMessage(b, WM_SETFONT, (WPARAM)hFontButton, TRUE);
    return b;
}

static void OnCreate(HWND hwnd) {
    g_app.hwndMain = hwnd;

    g_app.hwndBtnAddText = MakeOwnerButton(hwnd, ID_BTN_ADD_TEXT, 0,0,0,0, "Add Text / PDF");
    g_app.hwndBtnAddOcr  = MakeOwnerButton(hwnd, ID_BTN_ADD_OCR,  0,0,0,0, "Add Image / OCR");
    g_app.hwndBtnAnalyze = MakeOwnerButton(hwnd, ID_BTN_ANALYZE,  0,0,0,0, "ANALYZE PLAGIARISM");
    g_app.hwndBtnSave    = MakeOwnerButton(hwnd, ID_BTN_SAVE,     0,0,0,0, "Save Report");
    g_app.hwndBtnReset   = MakeOwnerButton(hwnd, ID_BTN_RESET,    0,0,0,0, "Reset");
    EnableWindow(g_app.hwndBtnAnalyze, FALSE);
    EnableWindow(g_app.hwndBtnSave, FALSE);

    g_app.hwndStatus = CreateWindowExA(0, "STATIC", "Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 100, 24, hwnd, (HMENU)(LONG_PTR)ID_STATUS,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(g_app.hwndStatus, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

    Layout(&g_app);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: OnCreate(hwnd); return 0;

        case WM_SIZE:
            Layout(&g_app);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            OnPaint(&g_app, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = (SHORT)LOWORD(lParam);
            pt.y = (SHORT)HIWORD(lParam);
            for (int i = 0; i < g_app.docCount; i++) {
                if (PtInRect(&g_app.removeRects[i], pt)) {
                    RemoveDoc(&g_app, i);
                    return 0;
                }
            }
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            const char* label = "Button";
            char buf[64];
            int len = GetWindowTextLengthA(dis->hwndItem);
            if (len > 0 && len < (int)sizeof(buf)) {
                GetWindowTextA(dis->hwndItem, buf, sizeof(buf));
                label = buf;
            }
            DrawOwnerButton(dis, label, hFontButton);
            return TRUE;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcCtl = (HDC)wParam;
            SetTextColor(hdcCtl, CLR_TEXT_PRIMARY);
            SetBkColor(hdcCtl, CLR_BACKGROUND);
            SetBkMode(hdcCtl, OPAQUE);
            static HBRUSH s_br = NULL;
            if (!s_br) s_br = CreateSolidBrush(CLR_BACKGROUND);
            return (LRESULT)s_br;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case ID_BTN_ADD_TEXT: HandleAdd(&g_app, FALSE); break;
                case ID_BTN_ADD_OCR:  HandleAdd(&g_app, TRUE);  break;
                case ID_BTN_ANALYZE:  RunAnalysis(&g_app); break;
                case ID_BTN_SAVE:     SaveReport(&g_app); break;
                case ID_BTN_RESET:    ResetAll(&g_app); break;
            }
            return 0;
        }

        case WM_DESTROY:
            for (int i = 0; i < g_app.docCount; i++) DocReset(&g_app.docs[i]);
            MatrixReset(&g_app);
            if (g_app.ocrInited && g_app.ocrAvailable) ShutdownOCR();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrev; (void)lpCmdLine;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    Theme_Init();

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BACKGROUND);
    wc.lpszClassName = APP_CLASS;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class", APP_TITLE, MB_ICONERROR);
        Theme_Cleanup();
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        APP_CLASS,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create main window", APP_TITLE, MB_ICONERROR);
        Theme_Cleanup();
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    Theme_Cleanup();
    return (int)msg.wParam;
}
