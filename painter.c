#include "painter.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Basic primitives
// ---------------------------------------------------------------------------

void DrawPanel(HDC hdc, RECT rect, COLORREF bg, COLORREF border) {
    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rect, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

void DrawSectionTitle(HDC hdc, int x, int y, const char* title, HFONT font) {
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    TextOutA(hdc, x, y, title, (int)strlen(title));

    SIZE sz;
    GetTextExtentPoint32A(hdc, title, (int)strlen(title), &sz);

    HPEN pen = CreatePen(PS_SOLID, 2, CLR_ACCENT);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y + sz.cy + 2, NULL);
    LineTo(hdc, x + sz.cx, y + sz.cy + 2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SelectObject(hdc, old);
}

void DrawProgressBar(HDC hdc, RECT rect, double percent, COLORREF color) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    HBRUSH bgBr = CreateSolidBrush(CLR_PROGRESS_BG);
    FillRect(hdc, &rect, bgBr);
    DeleteObject(bgBr);

    int width = rect.right - rect.left;
    int filled = (int)((width * percent) / 100.0);

    RECT fill = rect;
    fill.right = rect.left + filled;
    HBRUSH fillBr = CreateSolidBrush(color);
    FillRect(hdc, &fill, fillBr);
    DeleteObject(fillBr);

    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

COLORREF MatrixCellColor(double pct) {
    if (pct >= 70.0) return CLR_CELL_HIGH;
    if (pct >= 40.0) return CLR_CELL_MED;
    return CLR_CELL_LOW;
}

// ---------------------------------------------------------------------------
// File list panel
// ---------------------------------------------------------------------------

void DrawFileListPanel(HDC hdc, RECT rect, FileListRow* rows, int rowCount,
                       int maxRows, int canAdd) {
    (void)canAdd;
    DrawPanel(hdc, rect, CLR_PANEL, CLR_BORDER);

    int x = rect.left + 16;
    int y = rect.top  + 10;

    char header[64];
    snprintf(header, sizeof(header), "FILES TO COMPARE   (%d / %d)", rowCount, maxRows);
    DrawSectionTitle(hdc, x, y, header, hFontSection);
    y += 32;

    SetBkMode(hdc, TRANSPARENT);

    if (rowCount == 0) {
        SelectObject(hdc, hFontLabel);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        const char* hint = "No files yet. Click 'Add Text/PDF' or 'Add Image / OCR' to load 2 or more documents.";
        TextOutA(hdc, x, y, hint, (int)strlen(hint));
        return;
    }

    int rowH = 24;
    int listLeft = x;
    int listRight = rect.right - 16;
    int removeW = 70;
    int removeGap = 12;

    SelectObject(hdc, hFontLabel);

    for (int i = 0; i < rowCount; i++) {
        int ry = y + i * rowH;

        // Alternating row stripe for readability
        if (i % 2 == 1) {
            RECT stripe;
            stripe.left   = listLeft;
            stripe.top    = ry;
            stripe.right  = listRight;
            stripe.bottom = ry + rowH;
            HBRUSH br = CreateSolidBrush(CLR_PANEL_ALT);
            FillRect(hdc, &stripe, br);
            DeleteObject(br);
        }

        // Index
        char idx[16];
        snprintf(idx, sizeof(idx), "%d.", rows[i].index);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        TextOutA(hdc, listLeft + 4, ry + 4, idx, (int)strlen(idx));

        // Name (+ OCR tag)
        char label[320];
        if (rows[i].is_ocr) {
            snprintf(label, sizeof(label), "%s   [OCR]   %d chars",
                     rows[i].name ? rows[i].name : "(unnamed)", rows[i].char_count);
        } else {
            snprintf(label, sizeof(label), "%s   %d chars",
                     rows[i].name ? rows[i].name : "(unnamed)", rows[i].char_count);
        }
        SetTextColor(hdc, CLR_TEXT_PRIMARY);

        RECT nameRect;
        nameRect.left   = listLeft + 38;
        nameRect.top    = ry + 3;
        nameRect.right  = listRight - removeW - removeGap;
        nameRect.bottom = ry + rowH - 3;
        DrawTextA(hdc, label, -1, &nameRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Remove button (hand-painted; main.c hit-tests against rows[i].removeRect)
        RECT rb;
        rb.left   = listRight - removeW;
        rb.top    = ry + 3;
        rb.right  = listRight - 2;
        rb.bottom = ry + rowH - 3;
        rows[i].removeRect = rb;

        HBRUSH bbr = CreateSolidBrush(CLR_DANGER);
        FillRect(hdc, &rb, bbr);
        DeleteObject(bbr);

        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        DrawTextA(hdc, "Remove", -1, &rb,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// ---------------------------------------------------------------------------
// Matrix results panel
// ---------------------------------------------------------------------------

static const char* TruncForHeader(const char* s, char* buf, int bufSz, int max) {
    if (!s) { buf[0] = '\0'; return buf; }
    int n = (int)strlen(s);
    if (n <= max) {
        snprintf(buf, bufSz, "%s", s);
        return buf;
    }
    int keep = max - 1;
    if (keep < 1) keep = 1;
    snprintf(buf, bufSz, "%.*s…", keep, s);
    return buf;
}

static void DrawMatrixGrid(HDC hdc, RECT area, const MatrixResult* r) {
    int n = r->count;
    if (n <= 0) return;

    // Cell sizing — fit all rows + 1 header row, all cols + 1 header col.
    int areaW = area.right - area.left;
    int areaH = area.bottom - area.top;

    int rowH = 26;
    int headerColW = 130;
    int cellW = (areaW - headerColW - 8) / n;
    if (cellW < 60)  cellW = 60;
    if (cellW > 110) cellW = 110;

    int totalH = rowH * (n + 1);
    if (totalH > areaH) {
        rowH = areaH / (n + 1);
        if (rowH < 18) rowH = 18;
    }

    int gridLeft = area.left;
    int gridTop  = area.top;

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, hFontLabel);

    // Header row (column numbers)
    for (int j = 0; j < n; j++) {
        RECT hc;
        hc.left   = gridLeft + headerColW + j * cellW;
        hc.top    = gridTop;
        hc.right  = hc.left + cellW;
        hc.bottom = gridTop + rowH;

        HBRUSH br = CreateSolidBrush(CLR_CELL_HEADER);
        FillRect(hdc, &hc, br);
        DeleteObject(br);

        char lbl[16];
        snprintf(lbl, sizeof(lbl), "#%d", j + 1);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        DrawTextA(hdc, lbl, -1, &hc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Header column (filenames) + data cells
    char nameBuf[64];
    for (int i = 0; i < n; i++) {
        int ry = gridTop + (i + 1) * rowH;

        // Row header
        RECT rh;
        rh.left   = gridLeft;
        rh.top    = ry;
        rh.right  = gridLeft + headerColW;
        rh.bottom = ry + rowH;

        HBRUSH br = CreateSolidBrush(CLR_CELL_HEADER);
        FillRect(hdc, &rh, br);
        DeleteObject(br);

        const char* shown = TruncForHeader(r->names[i], nameBuf, sizeof(nameBuf), 16);
        char rowLbl[80];
        snprintf(rowLbl, sizeof(rowLbl), "%d. %s", i + 1, shown);
        rh.left += 6;
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        DrawTextA(hdc, rowLbl, -1, &rh, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Data cells
        for (int j = 0; j < n; j++) {
            RECT c;
            c.left   = gridLeft + headerColW + j * cellW;
            c.top    = ry;
            c.right  = c.left + cellW;
            c.bottom = ry + rowH;

            if (i == j) {
                HBRUSH d = CreateSolidBrush(CLR_CELL_DIAG);
                FillRect(hdc, &c, d);
                DeleteObject(d);

                SetTextColor(hdc, CLR_TEXT_MUTED);
                DrawTextA(hdc, "—", -1, &c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                double pct = r->matrix[i * n + j];
                COLORREF bg = MatrixCellColor(pct);
                HBRUSH cb = CreateSolidBrush(bg);
                FillRect(hdc, &c, cb);
                DeleteObject(cb);

                // Highlight the top pair
                if ((i == r->top_a && j == r->top_b) || (i == r->top_b && j == r->top_a)) {
                    HPEN hp = CreatePen(PS_SOLID, 2, CLR_TEXT_PRIMARY);
                    HPEN oldP = (HPEN)SelectObject(hdc, hp);
                    HBRUSH oldB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, c.left + 1, c.top + 1, c.right - 1, c.bottom - 1);
                    SelectObject(hdc, oldP);
                    SelectObject(hdc, oldB);
                    DeleteObject(hp);
                }

                char txt[16];
                snprintf(txt, sizeof(txt), "%.1f%%", pct);
                SetTextColor(hdc, CLR_TEXT_PRIMARY);
                DrawTextA(hdc, txt, -1, &c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    // Grid outline
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, gridLeft, gridTop,
                   gridLeft + headerColW + cellW * n,
                   gridTop  + rowH * (n + 1));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

void DrawMatrixResultsPanel(HDC hdc, RECT rect, const MatrixResult* r) {
    DrawPanel(hdc, rect, CLR_PANEL, CLR_BORDER);

    int x = rect.left + 16;
    int y = rect.top  + 12;

    int pairCount = r->count >= 2 ? r->count * (r->count - 1) / 2 : 0;
    char hdr[128];
    if (r->has_result) {
        snprintf(hdr, sizeof(hdr), "RESULTS — %d files, %d pairs", r->count, pairCount);
    } else {
        snprintf(hdr, sizeof(hdr), "RESULTS");
    }
    DrawSectionTitle(hdc, x, y, hdr, hFontSection);
    y += 32;

    SetBkMode(hdc, TRANSPARENT);

    if (!r->has_result) {
        SelectObject(hdc, hFontLabel);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        const char* hint = "Add at least 2 documents and press ANALYZE PLAGIARISM.";
        TextOutA(hdc, x, y, hint, (int)strlen(hint));
        return;
    }

    // Headline: top pair
    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "Highest pair", 12);

    const char* na = r->names && r->names[r->top_a] ? r->names[r->top_a] : "?";
    const char* nb = r->names && r->names[r->top_b] ? r->names[r->top_b] : "?";
    char headline[512];
    snprintf(headline, sizeof(headline), "%s  vs  %s    %.1f%%",
             na, nb, r->top_percentage);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, hFontVerdict);
    TextOutA(hdc, x + 110, y - 2, headline, (int)strlen(headline));
    y += 28;

    // Verdict + LCS length
    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "Verdict", 7);
    SelectObject(hdc, hFontVerdict);
    SetTextColor(hdc, r->top_verdict_color);
    TextOutA(hdc, x + 110, y - 2, r->top_verdict ? r->top_verdict : "",
             r->top_verdict ? (int)strlen(r->top_verdict) : 0);
    y += 24;

    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "LCS length", 10);
    char line[128];
    snprintf(line, sizeof(line), "%d characters", r->top_lcs_length);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    TextOutA(hdc, x + 110, y, line, (int)strlen(line));
    y += 24;

    // Divider
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, rect.right - 16, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    y += 8;

    // Matrix grid takes the bulk of the panel
    RECT matrixArea;
    matrixArea.left   = x;
    matrixArea.top    = y;
    matrixArea.right  = rect.right - 16;
    matrixArea.bottom = rect.bottom - 12;

    // Reserve some space at the bottom for matched-segment preview if it fits
    int segH = 70;
    if (matrixArea.bottom - matrixArea.top > segH + 200 && r->top_lcs_preview && *r->top_lcs_preview) {
        matrixArea.bottom -= segH;
    } else {
        segH = 0;
    }

    DrawMatrixGrid(hdc, matrixArea, r);

    // Matched segment under the matrix (when there is room)
    if (segH > 0) {
        int sy = matrixArea.bottom + 6;
        SelectObject(hdc, hFontLabel);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        TextOutA(hdc, x, sy, "Matching segment (top pair):", 28);

        RECT pr;
        pr.left   = x;
        pr.top    = sy + 18;
        pr.right  = rect.right - 16;
        pr.bottom = rect.bottom - 8;

        SelectObject(hdc, hFontMono);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        DrawTextA(hdc, r->top_lcs_preview, -1, &pr,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_EDITCONTROL);
    }
}

// ---------------------------------------------------------------------------
// Owner-drawn button
// ---------------------------------------------------------------------------

void DrawOwnerButton(LPDRAWITEMSTRUCT dis, const char* label, HFONT font) {
    BOOL isPressed  = (dis->itemState & ODS_SELECTED) != 0;
    BOOL isFocused  = (dis->itemState & ODS_FOCUS)    != 0;
    BOOL isHot      = (dis->itemState & ODS_HOTLIGHT) != 0;
    BOOL isDisabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg = CLR_ACCENT;
    if (isDisabled) bg = CLR_PANEL_ALT;
    else if (isPressed) bg = CLR_ACCENT_PRESS;
    else if (isHot || isFocused) bg = CLR_ACCENT_HOVER;

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, isDisabled ? CLR_BORDER : CLR_ACCENT_HOVER);
    HPEN oldPen = (HPEN)SelectObject(dis->hDC, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top,
                        dis->rcItem.right, dis->rcItem.bottom);
    SelectObject(dis->hDC, oldPen);
    SelectObject(dis->hDC, oldBr);
    DeleteObject(pen);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, isDisabled ? CLR_TEXT_MUTED : CLR_TEXT_PRIMARY);
    HFONT oldFont = (HFONT)SelectObject(dis->hDC, font);
    DrawTextA(dis->hDC, label, -1, &dis->rcItem,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dis->hDC, oldFont);
}
