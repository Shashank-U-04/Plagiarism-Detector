#include "painter.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

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

void DrawResultsPanel(HDC hdc, RECT rect, const AnalysisResult* result) {
    DrawPanel(hdc, rect, CLR_PANEL, CLR_BORDER);

    int x = rect.left + 16;
    int y = rect.top  + 12;

    DrawSectionTitle(hdc, x, y, "RESULTS", hFontSection);
    y += 32;

    SetBkMode(hdc, TRANSPARENT);

    if (!result->has_result) {
        SelectObject(hdc, hFontLabel);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        const char* hint = "Load two documents and press ANALYZE PLAGIARISM.";
        TextOutA(hdc, x, y, hint, (int)strlen(hint));
        return;
    }

    char line[512];

    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "Similarity", 10);

    RECT bar;
    bar.left   = x + 110;
    bar.right  = rect.right - 120;
    bar.top    = y + 2;
    bar.bottom = y + 20;
    DrawProgressBar(hdc, bar, result->percentage, result->verdict_color);

    snprintf(line, sizeof(line), "%.1f%%", result->percentage);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    SelectObject(hdc, hFontVerdict);
    TextOutA(hdc, bar.right + 12, y - 2, line, (int)strlen(line));

    y += 36;

    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "Verdict", 7);
    SelectObject(hdc, hFontVerdict);
    SetTextColor(hdc, result->verdict_color);
    TextOutA(hdc, x + 110, y - 2, result->verdict, (int)strlen(result->verdict));
    y += 28;

    SelectObject(hdc, hFontLabel);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "LCS Length", 10);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    snprintf(line, sizeof(line), "%d characters", result->lcs_length);
    TextOutA(hdc, x + 110, y, line, (int)strlen(line));
    y += 22;

    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "Doc lengths", 11);
    SetTextColor(hdc, CLR_TEXT_PRIMARY);
    snprintf(line, sizeof(line), "Doc 1: %d   Doc 2: %d", result->len1, result->len2);
    TextOutA(hdc, x + 110, y, line, (int)strlen(line));
    y += 28;

    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, rect.right - 16, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    y += 8;

    SetTextColor(hdc, CLR_TEXT_MUTED);
    TextOutA(hdc, x, y, "Matching segment (LCS preview):", 31);
    y += 20;

    if (result->lcs_preview && *result->lcs_preview) {
        RECT pr;
        pr.left   = x;
        pr.top    = y;
        pr.right  = rect.right - 16;
        pr.bottom = rect.bottom - 12;

        SelectObject(hdc, hFontMono);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        DrawTextA(hdc, result->lcs_preview, -1, &pr,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_EDITCONTROL);
    }
}

void DrawOwnerButton(LPDRAWITEMSTRUCT dis, const char* label, HFONT font) {
    BOOL isPressed = (dis->itemState & ODS_SELECTED) != 0;
    BOOL isFocused = (dis->itemState & ODS_FOCUS)    != 0;
    BOOL isHot     = (dis->itemState & ODS_HOTLIGHT) != 0;
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
