#ifndef THEME_H
#define THEME_H

#include <windows.h>

#define CLR_BACKGROUND    RGB(18,  18,  30)
#define CLR_PANEL         RGB(28,  28,  45)
#define CLR_PANEL_ALT     RGB(36,  36,  56)
#define CLR_ACCENT        RGB(99,  102, 241)
#define CLR_ACCENT_HOVER  RGB(129, 132, 255)
#define CLR_ACCENT_PRESS  RGB(79,   82, 211)
#define CLR_TEXT_PRIMARY  RGB(240, 240, 255)
#define CLR_TEXT_MUTED    RGB(140, 140, 170)
#define CLR_SUCCESS       RGB(34,  197,  94)
#define CLR_WARNING       RGB(234, 179,   8)
#define CLR_DANGER        RGB(239,  68,  68)
#define CLR_BORDER        RGB(55,   55,  80)
#define CLR_PROGRESS_BG   RGB(45,   45,  65)

#define CLR_CELL_HIGH     RGB(120,  35,  40)
#define CLR_CELL_MED      RGB(120,  90,  20)
#define CLR_CELL_LOW      RGB(30,   90,  50)
#define CLR_CELL_DIAG     RGB(40,   40,  60)
#define CLR_CELL_HEADER   RGB(50,   50,  72)

extern HFONT hFontTitle;
extern HFONT hFontSection;
extern HFONT hFontLabel;
extern HFONT hFontMono;
extern HFONT hFontButton;
extern HFONT hFontVerdict;

void Theme_Init(void);
void Theme_Cleanup(void);

#endif // THEME_H
