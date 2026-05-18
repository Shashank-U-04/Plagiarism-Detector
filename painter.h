#ifndef PAINTER_H
#define PAINTER_H

#include <windows.h>

typedef struct {
    double percentage;
    int    lcs_length;
    int    len1;
    int    len2;
    const char* verdict;
    COLORREF verdict_color;
    const char* lcs_preview;
    int has_result;
} AnalysisResult;

void DrawPanel(HDC hdc, RECT rect, COLORREF bg, COLORREF border);
void DrawSectionTitle(HDC hdc, int x, int y, const char* title, HFONT font);
void DrawProgressBar(HDC hdc, RECT rect, double percent, COLORREF color);
void DrawResultsPanel(HDC hdc, RECT rect, const AnalysisResult* result);

void DrawOwnerButton(LPDRAWITEMSTRUCT dis, const char* label, HFONT font);

#endif // PAINTER_H
