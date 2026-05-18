#ifndef PAINTER_H
#define PAINTER_H

#include <windows.h>

// A single row in the file-list panel. `removeRect` is filled by DrawFileListPanel
// so the main window can hit-test mouse clicks.
typedef struct {
    int         index;       // 1-based display index
    const char* name;        // basename
    int         is_ocr;      // 1 if loaded via OCR
    int         char_count;  // normalized text length
    RECT        removeRect;  // OUT: filled by DrawFileListPanel
} FileListRow;

// Matrix-mode result data passed to DrawMatrixResultsPanel.
typedef struct {
    int           count;             // number of files (matrix is count x count)
    const char* const* names;        // basenames, length == count
    const double* matrix;            // row-major count*count, percentages [0,100]

    int           top_a;             // index of highest-similarity pair
    int           top_b;
    double        top_percentage;
    int           top_lcs_length;
    const char*   top_verdict;
    COLORREF      top_verdict_color;
    const char*   top_lcs_preview;

    int           has_result;
} MatrixResult;

void DrawPanel(HDC hdc, RECT rect, COLORREF bg, COLORREF border);
void DrawSectionTitle(HDC hdc, int x, int y, const char* title, HFONT font);
void DrawProgressBar(HDC hdc, RECT rect, double percent, COLORREF color);

// Render the file-list panel. Writes per-row remove button rects (in window coords)
// into rows[i].removeRect. canAdd indicates whether the user can add more (affects hint).
void DrawFileListPanel(HDC hdc, RECT rect, FileListRow* rows, int rowCount,
                       int maxRows, int canAdd);

// Render the matrix + headline + matched-segment panel.
void DrawMatrixResultsPanel(HDC hdc, RECT rect, const MatrixResult* result);

void DrawOwnerButton(LPDRAWITEMSTRUCT dis, const char* label, HFONT font);

// Map a similarity percentage to a cell background color band.
COLORREF MatrixCellColor(double pct);

#endif // PAINTER_H
