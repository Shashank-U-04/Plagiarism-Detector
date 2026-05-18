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

#define WIN_W       1100
#define WIN_H       760

// Control IDs
#define ID_BTN_LOAD1     101
#define ID_BTN_OCR1      102
#define ID_BTN_LOAD2     103
#define ID_BTN_OCR2      104
#define ID_BTN_ANALYZE   105
#define ID_BTN_SAVE      106
#define ID_BTN_RESET     107
#define ID_PREVIEW1      108
#define ID_PREVIEW2      109
#define ID_STATUS        110

typedef struct {
    char  path[MAX_PATH];
    char  name[256];
    char* text;        // raw extracted text (malloc'd)
    char* normalized;  // normalized text (malloc'd)
} DocState;

typedef struct {
    HWND hwndMain;
    HWND hwndPreview1;
    HWND hwndPreview2;
    HWND hwndStatus;
    HWND hwndBtnLoad1;
    HWND hwndBtnOcr1;
    HWND hwndBtnLoad2;
    HWND hwndBtnOcr2;
    HWND hwndBtnAnalyze;
    HWND hwndBtnSave;
    HWND hwndBtnReset;

    DocState doc1;
    DocState doc2;

    AnalysisResult result;
    char* lcs_text;     // owned string for result.lcs_preview
    char  verdict_buf[64];

    RECT panel1Rect;
    RECT panel2Rect;
    RECT resultsRect;
    RECT statusRect;

    BOOL ocrInited;
    BOOL ocrAvailable;
} App;

static App g_app;

// -------- utility helpers --------

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
}

static void ResultReset(App* a) {
    free(a->lcs_text); a->lcs_text = NULL;
    memset(&a->result, 0, sizeof(a->result));
    a->verdict_buf[0] = '\0';
}

static void SetStatus(App* a, const char* msg) {
    if (a->hwndStatus) SetWindowTextA(a->hwndStatus, msg);
}

static void SetPreviewText(HWND hEdit, const char* text, const char* fileName) {
    if (!hEdit) return;
    if (!text || !*text) {
        SetWindowTextA(hEdit, "(no document loaded)");
        return;
    }
    // Show up to first 8000 chars with a header line
    size_t n = strlen(text);
    if (n > 8000) n = 8000;
    char* buf = (char*)malloc(n + 256);
    if (!buf) return;
    int pre = snprintf(buf, 256, "[%s]\r\n\r\n", fileName ? fileName : "");
    memcpy(buf + pre, text, n);
    buf[pre + n] = '\0';
    // Convert lone \n to \r\n for Edit control display
    char* out = (char*)malloc(strlen(buf) * 2 + 1);
    if (!out) { free(buf); return; }
    char* o = out;
    for (char* p = buf; *p; p++) {
        if (*p == '\n' && (p == buf || *(p-1) != '\r')) { *o++ = '\r'; *o++ = '\n'; }
        else *o++ = *p;
    }
    *o = '\0';
    SetWindowTextA(hEdit, out);
    free(out);
    free(buf);
}

static const char* VerdictFor(double pct, COLORREF* color) {
    if (pct >= 70.0)      { *color = CLR_DANGER;  return "HIGH - Likely Plagiarism"; }
    else if (pct >= 40.0) { *color = CLR_WARNING; return "MEDIUM - Suspicious"; }
    else                  { *color = CLR_SUCCESS; return "LOW - Looks Original"; }
}

// -------- loaders --------

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
    } else if (strcmp(ext, ".pdf") == 0) {
        SetStatus(a, "Extracting PDF text ...");
        text = ExtractTextFromPDF(path);
    } else { // txt and unknown -> plain read
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

// -------- analyze --------

static void RunAnalysis(App* a) {
    if (!a->doc1.normalized || !a->doc2.normalized) {
        SetStatus(a, "Load BOTH documents before analyzing");
        return;
    }
    SetStatus(a, "Analyzing ...");

    ResultReset(a);

    int lcs_len = 0;
    char* lcs = NULL;
    double pct = AnalyzeLCS(a->doc1.normalized, a->doc2.normalized, &lcs_len, &lcs);

    COLORREF vc;
    const char* v = VerdictFor(pct, &vc);
    strncpy(a->verdict_buf, v, sizeof(a->verdict_buf) - 1);
    a->verdict_buf[sizeof(a->verdict_buf) - 1] = '\0';

    a->result.percentage    = pct;
    a->result.lcs_length    = lcs_len;
    a->result.len1          = (int)strlen(a->doc1.normalized);
    a->result.len2          = (int)strlen(a->doc2.normalized);
    a->result.verdict       = a->verdict_buf;
    a->result.verdict_color = vc;
    a->lcs_text             = lcs;
    a->result.lcs_preview   = lcs;
    a->result.has_result    = 1;

    EnableWindow(a->hwndBtnSave, TRUE);
    InvalidateRect(a->hwndMain, &a->resultsRect, TRUE);
    SetStatus(a, "Analysis complete");
}

static void SaveReport(App* a) {
    if (!a->result.has_result) {
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

    fprintf(f, "PLAGIARISM DETECTION REPORT\r\n");
    fprintf(f, "Generated : %s\r\n", ts);
    fprintf(f, "----------------------------------------\r\n");
    fprintf(f, "Document 1 : %s\r\n", a->doc1.name);
    fprintf(f, "             %s\r\n", a->doc1.path);
    fprintf(f, "Document 2 : %s\r\n", a->doc2.name);
    fprintf(f, "             %s\r\n", a->doc2.path);
    fprintf(f, "----------------------------------------\r\n");
    fprintf(f, "Similarity : %.2f %%\r\n", a->result.percentage);
    fprintf(f, "Verdict    : %s\r\n", a->result.verdict);
    fprintf(f, "LCS length : %d characters\r\n", a->result.lcs_length);
    fprintf(f, "Doc 1 len  : %d\r\n", a->result.len1);
    fprintf(f, "Doc 2 len  : %d\r\n", a->result.len2);
    fprintf(f, "----------------------------------------\r\n");
    fprintf(f, "Matching LCS preview:\r\n");
    if (a->lcs_text) fputs(a->lcs_text, f);
    fprintf(f, "\r\n");
    fclose(f);
    SetStatus(a, "Report saved");
}

static void ResetAll(App* a) {
    DocReset(&a->doc1);
    DocReset(&a->doc2);
    ResultReset(a);
    SetPreviewText(a->hwndPreview1, NULL, NULL);
    SetPreviewText(a->hwndPreview2, NULL, NULL);
    EnableWindow(a->hwndBtnSave, FALSE);
    InvalidateRect(a->hwndMain, NULL, TRUE);
    SetStatus(a, "Reset");
}

// -------- layout --------

static void Layout(App* a) {
    RECT cr;
    GetClientRect(a->hwndMain, &cr);
    int W = cr.right - cr.left;
    int H = cr.bottom - cr.top;

    int margin = 16;
    int gap = 16;
    int titleH = 56;
    int panelW = (W - margin * 2 - gap) / 2;
    int panelH = 280;
    int panelY = titleH;

    int btnH = 32;
    int btnGap = 8;
    int previewH = panelH - 16 - btnH * 2 - btnGap - 32; // header + 2 buttons

    a->panel1Rect.left   = margin;
    a->panel1Rect.top    = panelY;
    a->panel1Rect.right  = margin + panelW;
    a->panel1Rect.bottom = panelY + panelH;

    a->panel2Rect.left   = margin + panelW + gap;
    a->panel2Rect.top    = panelY;
    a->panel2Rect.right  = margin + panelW + gap + panelW;
    a->panel2Rect.bottom = panelY + panelH;

    // Panel 1 children
    int p1x = a->panel1Rect.left + 12;
    int p1y = a->panel1Rect.top + 36;
    int pw  = panelW - 24;
    MoveWindow(a->hwndPreview1, p1x, p1y, pw, previewH, TRUE);
    int by = p1y + previewH + 8;
    int halfW = (pw - btnGap) / 2;
    MoveWindow(a->hwndBtnLoad1, p1x,                 by, halfW, btnH, TRUE);
    MoveWindow(a->hwndBtnOcr1,  p1x + halfW + btnGap, by, halfW, btnH, TRUE);

    // Panel 2 children
    int p2x = a->panel2Rect.left + 12;
    int p2y = a->panel2Rect.top + 36;
    MoveWindow(a->hwndPreview2, p2x, p2y, pw, previewH, TRUE);
    int by2 = p2y + previewH + 8;
    MoveWindow(a->hwndBtnLoad2, p2x,                 by2, halfW, btnH, TRUE);
    MoveWindow(a->hwndBtnOcr2,  p2x + halfW + btnGap, by2, halfW, btnH, TRUE);

    // Analyze button row
    int anaY = panelY + panelH + 12;
    int anaW = 260;
    int anaH = 42;
    MoveWindow(a->hwndBtnAnalyze, (W - anaW) / 2, anaY, anaW, anaH, TRUE);

    // Results panel
    int resY = anaY + anaH + 14;
    int resH = H - resY - margin - 56;
    if (resH < 160) resH = 160;
    a->resultsRect.left   = margin;
    a->resultsRect.top    = resY;
    a->resultsRect.right  = W - margin;
    a->resultsRect.bottom = resY + resH;

    // Bottom toolbar (save / reset / status)
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

// -------- painting --------

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
    const char* sub = "Compare two documents using LCS dynamic programming";
    TextOutA(hdc, 16, 40, sub, (int)strlen(sub));

    // Panels
    DrawPanel(hdc, a->panel1Rect, CLR_PANEL, CLR_BORDER);
    DrawPanel(hdc, a->panel2Rect, CLR_PANEL, CLR_BORDER);

    char hdr1[320], hdr2[320];
    if (a->doc1.name[0])
        snprintf(hdr1, sizeof(hdr1), "DOCUMENT 1 - %s", a->doc1.name);
    else
        snprintf(hdr1, sizeof(hdr1), "DOCUMENT 1");
    if (a->doc2.name[0])
        snprintf(hdr2, sizeof(hdr2), "DOCUMENT 2 - %s", a->doc2.name);
    else
        snprintf(hdr2, sizeof(hdr2), "DOCUMENT 2");

    DrawSectionTitle(hdc, a->panel1Rect.left + 12, a->panel1Rect.top + 10, hdr1, hFontSection);
    DrawSectionTitle(hdc, a->panel2Rect.left + 12, a->panel2Rect.top + 10, hdr2, hFontSection);

    // Results
    DrawResultsPanel(hdc, a->resultsRect, &a->result);

    SelectObject(hdc, old);
}

// -------- WndProc plumbing --------

static HWND MakeOwnerButton(HWND parent, int id, int x, int y, int w, int h, const char* text) {
    HWND b = CreateWindowExA(0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(LONG_PTR)id,
        (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE), NULL);
    SendMessage(b, WM_SETFONT, (WPARAM)hFontButton, TRUE);
    return b;
}

static HWND MakeEditPreview(HWND parent, int id) {
    HWND e = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, 100, 100, parent, (HMENU)(LONG_PTR)id,
        (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE), NULL);
    SendMessage(e, WM_SETFONT, (WPARAM)hFontMono, TRUE);
    SetWindowTextA(e, "(no document loaded)");
    return e;
}

static void OnCreate(HWND hwnd) {
    g_app.hwndMain = hwnd;

    g_app.hwndPreview1   = MakeEditPreview(hwnd, ID_PREVIEW1);
    g_app.hwndPreview2   = MakeEditPreview(hwnd, ID_PREVIEW2);

    g_app.hwndBtnLoad1   = MakeOwnerButton(hwnd, ID_BTN_LOAD1,   0,0,0,0, "Load Text / PDF");
    g_app.hwndBtnOcr1    = MakeOwnerButton(hwnd, ID_BTN_OCR1,    0,0,0,0, "Load Image / OCR");
    g_app.hwndBtnLoad2   = MakeOwnerButton(hwnd, ID_BTN_LOAD2,   0,0,0,0, "Load Text / PDF");
    g_app.hwndBtnOcr2    = MakeOwnerButton(hwnd, ID_BTN_OCR2,    0,0,0,0, "Load Image / OCR");
    g_app.hwndBtnAnalyze = MakeOwnerButton(hwnd, ID_BTN_ANALYZE, 0,0,0,0, "ANALYZE PLAGIARISM");
    g_app.hwndBtnSave    = MakeOwnerButton(hwnd, ID_BTN_SAVE,    0,0,0,0, "Save Report");
    g_app.hwndBtnReset   = MakeOwnerButton(hwnd, ID_BTN_RESET,   0,0,0,0, "Reset");
    EnableWindow(g_app.hwndBtnSave, FALSE);

    g_app.hwndStatus = CreateWindowExA(0, "STATIC", "Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 100, 24, hwnd, (HMENU)(LONG_PTR)ID_STATUS,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(g_app.hwndStatus, WM_SETFONT, (WPARAM)hFontLabel, TRUE);

    Layout(&g_app);
}

static void HandleLoad(App* a, DocState* d, BOOL useOcr, HWND focusFor) {
    char path[MAX_PATH] = "";
    BOOL ok = useOcr
        ? PickImageFile(a->hwndMain, path, sizeof(path))
        : PickDocumentFile(a->hwndMain, path, sizeof(path));
    if (!ok) return;
    if (LoadDocFromPath(a, d, path, useOcr)) {
        SetPreviewText(focusFor, d->text, d->name);
        ResultReset(a);
        EnableWindow(a->hwndBtnSave, FALSE);
        InvalidateRect(a->hwndMain, NULL, TRUE);
        char msg[512];
        snprintf(msg, sizeof(msg), "Loaded %s (%zu chars)", d->name,
                 d->normalized ? strlen(d->normalized) : 0);
        SetStatus(a, msg);
    }
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

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdcCtl = (HDC)wParam;
            SetTextColor(hdcCtl, CLR_TEXT_PRIMARY);
            SetBkColor(hdcCtl, CLR_PANEL_ALT);
            SetBkMode(hdcCtl, OPAQUE);
            static HBRUSH s_br = NULL;
            if (!s_br) s_br = CreateSolidBrush(CLR_PANEL_ALT);
            return (LRESULT)s_br;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case ID_BTN_LOAD1: HandleLoad(&g_app, &g_app.doc1, FALSE, g_app.hwndPreview1); break;
                case ID_BTN_OCR1:  HandleLoad(&g_app, &g_app.doc1, TRUE,  g_app.hwndPreview1); break;
                case ID_BTN_LOAD2: HandleLoad(&g_app, &g_app.doc2, FALSE, g_app.hwndPreview2); break;
                case ID_BTN_OCR2:  HandleLoad(&g_app, &g_app.doc2, TRUE,  g_app.hwndPreview2); break;
                case ID_BTN_ANALYZE: RunAnalysis(&g_app); break;
                case ID_BTN_SAVE:    SaveReport(&g_app); break;
                case ID_BTN_RESET:   ResetAll(&g_app); break;
            }
            return 0;
        }

        case WM_DESTROY:
            DocReset(&g_app.doc1);
            DocReset(&g_app.doc2);
            ResultReset(&g_app);
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
