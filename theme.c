#include "theme.h"

HFONT hFontTitle   = NULL;
HFONT hFontSection = NULL;
HFONT hFontLabel   = NULL;
HFONT hFontMono    = NULL;
HFONT hFontButton  = NULL;
HFONT hFontVerdict = NULL;

static HFONT MakeFont(int heightPx, int weight, const char* face) {
    return CreateFontA(
        -heightPx, 0, 0, 0, weight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);
}

void Theme_Init(void) {
    hFontTitle   = MakeFont(24, FW_BOLD,   "Segoe UI");
    hFontSection = MakeFont(15, FW_SEMIBOLD,"Segoe UI");
    hFontLabel   = MakeFont(14, FW_NORMAL, "Segoe UI");
    hFontMono    = MakeFont(13, FW_NORMAL, "Consolas");
    hFontButton  = MakeFont(13, FW_BOLD,   "Segoe UI");
    hFontVerdict = MakeFont(16, FW_BOLD,   "Segoe UI");
}

void Theme_Cleanup(void) {
    if (hFontTitle)   { DeleteObject(hFontTitle);   hFontTitle   = NULL; }
    if (hFontSection) { DeleteObject(hFontSection); hFontSection = NULL; }
    if (hFontLabel)   { DeleteObject(hFontLabel);   hFontLabel   = NULL; }
    if (hFontMono)    { DeleteObject(hFontMono);    hFontMono    = NULL; }
    if (hFontButton)  { DeleteObject(hFontButton);  hFontButton  = NULL; }
    if (hFontVerdict) { DeleteObject(hFontVerdict); hFontVerdict = NULL; }
}
