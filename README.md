# Plagiarism Detector — Win32 GUI in pure C

A native Windows desktop application that compares two documents and reports a
plagiarism similarity score using the **Longest Common Subsequence (LCS)**
dynamic-programming algorithm. Built in plain C with the Win32 API — no Qt,
no Electron, no .NET runtime. Drawing is hand-rolled GDI with owner-drawn
controls and a dark theme.

Inputs are flexible: each of the two documents can be plain text, a PDF (any
page count, including 5+ pages), or an image (OCR via Tesseract).

> **Project type:** DAA (Design & Analysis of Algorithms) mini project.
> **Language:** C99. **UI:** Win32 + GDI. **Build:** MinGW64 / MSYS2.

---

## Table of contents

1. [Features](#features)
2. [Screenshots / UI layout](#ui-layout)
3. [How it works (algorithm)](#how-it-works)
4. [Project layout](#project-layout)
5. [Prerequisites](#prerequisites)
6. [Build](#build)
7. [Run](#run)
8. [Usage walkthrough](#usage-walkthrough)
9. [Troubleshooting](#troubleshooting)
10. [Implementation notes](#implementation-notes)
11. [License](#license)

---

## Features

- **Dual-document compare** in a single window — side-by-side panels with
  per-document previews.
- **Three input formats per document:**
  - **Text** — `.txt`
  - **PDF** — extracted via `pdftotext` (poppler), any page count
  - **Image** — `.png`, `.jpg`, `.jpeg`, `.bmp`, `.tif`, `.tiff` (OCR via Tesseract)
- **LCS dynamic programming** with two strategies:
  - Full-table DP when memory allows (~8M cells) so the *matching segment*
    can be reconstructed and shown in the result panel.
  - Rolling two-row DP (`O(min(n, m))` memory) as fallback for very large pairs.
- **Text normalization** before compare — lower-case, alphanumeric-only, single
  spaces — so cosmetic formatting differences do not inflate similarity.
- **3-band verdict:**

  | Similarity   | Verdict                       |
  |--------------|-------------------------------|
  | `>= 70 %`    | HIGH — Likely plagiarism      |
  | `40 – 69 %`  | MEDIUM — Suspicious           |
  | `< 40 %`     | LOW — Looks original          |

- **Save text report** with similarity %, verdict, LCS length, and matching
  segment.
- **Native dark theme** drawn by hand (no theme libraries) with custom
  owner-drawn buttons and progress bar.

---

## UI layout

```
+---------------------------------------------------------+
|  PLAGIARISM DETECTOR                                    |
|  Compare two documents using LCS dynamic programming    |
|                                                         |
|  +---------------------+   +---------------------+      |
|  | DOCUMENT 1          |   | DOCUMENT 2          |      |
|  | <preview>           |   | <preview>           |      |
|  | [Load Text/PDF]     |   | [Load Text/PDF]     |      |
|  | [Load Image / OCR]  |   | [Load Image / OCR]  |      |
|  +---------------------+   +---------------------+      |
|                                                         |
|              [ ANALYZE PLAGIARISM ]                     |
|                                                         |
|  RESULTS                                                |
|  Similarity   ##############__________  67.3 %          |
|  Verdict      MEDIUM - Suspicious                       |
|  LCS Length   412 characters                            |
|  Matching segment: the quick brown fox jumps over ...   |
|                                                         |
|  [ Save Report ]  [ Reset ]    Ready                    |
+---------------------------------------------------------+
```

---

## How it works

### LCS recurrence

```text
if text1[i-1] == text2[j-1]:
    dp[i][j] = 1 + dp[i-1][j-1]
else:
    dp[i][j] = max(dp[i-1][j], dp[i][j-1])

similarity (%) = LCS_length / max(len1, len2) * 100
```

### Two execution modes

| Function              | Memory          | Reconstructs match? | When used                |
|-----------------------|-----------------|---------------------|--------------------------|
| `AnalyzeLCS`          | `O(n * m)`      | Yes (full segment)  | When `n*m <= ~8M` cells  |
| `CalculateSimilarity` | `O(min(n, m))`  | No (length only)    | Fallback for huge inputs |

### Pipeline

```
input file  ─►  loader (text / pdftotext / Tesseract OCR)
            ─►  NormalizeText  (lowercase + alphanumeric + collapse spaces)
            ─►  AnalyzeLCS  ──or──  CalculateSimilarity
            ─►  similarity % + verdict + matched segment
            ─►  GDI render to results panel
```

---

## Project layout

```
Plagiarism-Detector/
├── main.c             WinMain, window procedure, layout, analyze workflow
├── theme.h / .c       Color palette and font handles
├── painter.h / .c     GDI helpers: panels, progress bar, results, buttons
├── algorithm.h / .c   NormalizeText + CalculateSimilarity + AnalyzeLCS
├── file_picker.h / .c Win32 GetOpenFileName / GetSaveFileName wrappers
├── pdf_extract.h / .c pdftotext / pdfinfo subprocess wrappers
├── ocr_engine.h / .c  Tesseract C-API wrapper
├── build.bat          MinGW64 build script
├── run.bat            Launcher that sets PATH + TESSDATA_PREFIX
├── .gitignore
└── README.md
```

Flat file structure on purpose — keeps the DAA project easy to read and audit.

---

## Prerequisites

- **Windows 10 / 11**
- **MSYS2** installed at `C:\msys64`
- MinGW64 toolchain + Tesseract + poppler:

```bat
pacman -S ^
    mingw-w64-x86_64-gcc ^
    mingw-w64-x86_64-tesseract-ocr ^
    mingw-w64-x86_64-tesseract-data-eng ^
    mingw-w64-x86_64-poppler
```

After install you should have:

| Tool          | Expected path                                       |
|---------------|-----------------------------------------------------|
| `gcc.exe`     | `C:\msys64\mingw64\bin\gcc.exe`                     |
| Tesseract DLL | `C:\msys64\mingw64\bin\libtesseract-*.dll`          |
| English data  | `C:\msys64\mingw64\share\tessdata\eng.traineddata`  |
| `pdftotext`   | `C:\msys64\mingw64\bin\pdftotext.exe`               |

---

## Build

From the project folder:

```bat
build.bat
```

This invokes:

```bat
gcc main.c theme.c painter.c algorithm.c file_picker.c pdf_extract.c ocr_engine.c ^
    -Wall -Wextra -O2 -std=c99 ^
    -I C:\msys64\mingw64\include ^
    -I C:\msys64\mingw64\include\tesseract ^
    -L C:\msys64\mingw64\lib ^
    -mwindows ^
    -o plagiarism_detector.exe ^
    -ltesseract -lgdi32 -lcomdlg32 -lcomctl32 -luser32 -lkernel32
```

Produces `plagiarism_detector.exe` (the binary itself is git-ignored).

---

## Run

> **Always launch via `run.bat`. Do not double-click the exe directly.**

```bat
run.bat
```

`run.bat` prepends `C:\msys64\mingw64\bin` to `PATH` and sets
`TESSDATA_PREFIX` before launching the executable. This is required because
the exe links to MinGW DLLs (`libtesseract-*.dll`, `libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, poppler libs) that live there. The build uses `-mwindows`
(no console attached), so a missing DLL produces *no error dialog* — the
process exits silently and no window appears.

If you prefer launching from PowerShell:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
$env:TESSDATA_PREFIX = "C:\msys64\mingw64\share\tessdata"
.\plagiarism_detector.exe
```

---

## Usage walkthrough

1. Launch via `run.bat`.
2. **DOCUMENT 1** panel:
   - Click **Load Text/PDF** for `.txt` or `.pdf`, or
   - Click **Load Image / OCR** for `.png/.jpg/.bmp/.tif`.
3. Repeat for **DOCUMENT 2**.
4. Click **ANALYZE PLAGIARISM**. The progress bar and verdict update.
5. Read the result: similarity %, verdict band, LCS length, matched segment.
6. **Save Report** writes a `.txt` summary. **Reset** clears both documents.

---

## Troubleshooting

| Symptom                                  | Cause                                                | Fix                                                                                  |
|------------------------------------------|------------------------------------------------------|--------------------------------------------------------------------------------------|
| Double-clicking exe → nothing happens    | MinGW DLLs not on PATH; `-mwindows` suppresses error | Launch via `run.bat`                                                                 |
| OCR returns empty text                   | `TESSDATA_PREFIX` not set or `eng.traineddata` missing | `set TESSDATA_PREFIX=C:\msys64\mingw64\share\tessdata`; reinstall `tesseract-data-eng` |
| PDF load fails or returns empty          | `pdftotext.exe` not on PATH                          | Install / reinstall `mingw-w64-x86_64-poppler`                                       |
| Build fails: `tesseract/capi.h not found`| Tesseract dev headers missing                        | `pacman -S mingw-w64-x86_64-tesseract-ocr`                                           |
| Build fails: `gcc not found`             | MinGW64 not in shell PATH                            | Use the MSYS2 MinGW64 shell, or add `C:\msys64\mingw64\bin` to PATH                  |
| App opens but window appears blank/black | GDI font load failed (rare)                          | Verify Segoe UI is installed (default on Win 10/11)                                  |

For verbose DLL-load debugging, temporarily drop `-mwindows` from
`build.bat` to attach a console and surface real errors at startup.

---

## Implementation notes

- **No external UI framework.** All controls (buttons, progress bar, panels)
  are owner-drawn via GDI in `painter.c`. The window procedure is in `main.c`.
- **Async-friendly analyze.** The analyze workflow is structured so OCR /
  PDF extraction (the slow path) happens on file load, not on the
  ANALYZE click; the LCS pass itself runs on the UI thread and completes
  in milliseconds for typical inputs.
- **Memory cap on full DP.** The full-table allocation is capped at roughly
  8 million cells (~64 MB at 8 bytes/cell). Pairs above this fall back to
  the rolling-row implementation; the match-segment field then shows the
  LCS length only.
- **Normalization is destructive on copy, not on source.** Both inputs
  are normalized into freshly allocated buffers — the previews still show
  the original text.

---

## License

Educational / academic project. No license attached — open an issue if you
want to reuse it in a different context.
