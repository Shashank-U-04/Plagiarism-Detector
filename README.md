# Plagiarism Detector — Win32 GUI in pure C

A native Windows desktop application that compares **up to 10 documents** in a
single pass and reports an N×N similarity matrix using the **Longest Common
Subsequence (LCS)** dynamic-programming algorithm. Built in plain C with the
Win32 API — no Qt, no Electron, no .NET runtime. Drawing is hand-rolled GDI
with owner-drawn controls and a dark theme.

Inputs are flexible: each document can be plain text, a **native-text PDF**,
a **handwritten / scanned PDF** (auto-detected and routed through OCR), or
an image (OCR via Tesseract).

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

- **N-way document compare** in a single window — add up to 10 files of mixed
  types, run one analyze pass, see every pairwise similarity.
- **Color-coded N×N matrix** in the results panel (red HIGH / yellow MEDIUM /
  green LOW). The top pair is outlined in accent color for quick spotting.
- **Four input modes per document:**
  - **Text** — `.txt`
  - **Native-text PDF** — extracted via `pdftotext` (poppler), any page count
  - **Handwritten / scanned PDF** — auto-detected when `pdftotext` yields
    < ~30 non-whitespace chars per page; the app then rasterizes each page
    via `pdftoppm` at 200 DPI and OCRs every page with Tesseract
  - **Image** — `.png`, `.jpg`, `.jpeg`, `.bmp`, `.tif`, `.tiff` (OCR via Tesseract)
- **Multi-select file load** — Ctrl- or Shift-click in the file dialog to add
  many files at once. The 10-file cap stops the batch cleanly and the status
  bar reports `Added X. Y skipped (10-file cap)`.
- **Live BUSY banner** — a prominent banner overlays the file list while OCR
  or PDF extraction runs, so you always see what the app is doing during the
  slow path instead of staring at a frozen window. Per-page progress
  (`OCR page k/N - filename.pdf`) updates between every page for handwritten
  PDFs, and the message pump is drained between pages/files so the OS keeps
  the window marked responsive even on long batches.
- **Per-row Remove** — drop any file from the list with a single click.
- **LCS dynamic programming** with two strategies:
  - Rolling two-row DP (`O(min(n, m))` memory) for every matrix cell — cheap.
  - Full-table DP (~8M cell cap) run **once** on the highest-scoring pair.
- **Text normalization** before compare — lower-case, alphanumeric-only, single
  spaces — so cosmetic formatting differences do not inflate similarity.
- **3-band verdict** (applied to the top pair):

  | Similarity   | Verdict                       |
  |--------------|-------------------------------|
  | `>= 70 %`    | HIGH — Likely plagiarism      |
  | `40 – 69 %`  | MEDIUM — Suspicious           |
  | `< 40 %`     | LOW — Looks original          |

- **Save text report** with the file index, highest-pair verdict, and the
  full ASCII similarity matrix.
- **Native dark theme** drawn by hand (no theme libraries) with custom
  owner-drawn buttons and matrix grid.

---

## UI layout

```
+----------------------------------------------------------------------------+
|  PLAGIARISM DETECTOR                                                       |
|  Compare up to 10 documents using LCS dynamic programming                  |
|                                                                            |
|  +----------------------------------------------------------------------+  |
|  | FILES TO COMPARE   (4 / 10)                                          |  |
|  | 1.  essay_alice.txt          5210 chars               [ Remove ]     |  |
|  | 2.  essay_bob.pdf            6041 chars               [ Remove ]     |  |
|  | 3.  essay_carol.png [OCR]    4988 chars               [ Remove ]     |  |
|  | 4.  essay_dan.txt            5102 chars               [ Remove ]     |  |
|  +----------------------------------------------------------------------+  |
|  [ Add Text / PDF ]  [ Add Image / OCR ]            [ ANALYZE PLAGIARISM ] |
|                                                                            |
|  RESULTS - 4 files, 6 pairs                                                |
|  Highest pair   essay_alice.txt  vs  essay_bob.pdf     78.2 %              |
|  Verdict        HIGH - Likely Plagiarism                                   |
|  ----------------------------------------------------------------------    |
|              1. alice...  2. bob...   3. carol...  4. dan...               |
|   1. alice      -          78.2 %      41.0 %       12.3 %                 |
|   2. bob       78.2 %       -          39.8 %       15.6 %                 |
|   3. carol     41.0 %      39.8 %       -           22.1 %                 |
|   4. dan       12.3 %      15.6 %      22.1 %        -                     |
|                                                                            |
|  [ Save Report ]  [ Reset ]                                Ready           |
+----------------------------------------------------------------------------+
```

Cell colors: **red** ≥ 70 % (HIGH), **yellow** 40 – 69 % (MEDIUM),
**green** < 40 % (LOW). The top-pair cell is outlined in accent color.

While OCR or PDF extraction runs, a BUSY banner overlays the file list,
positioned below the section header so the file rows above remain visible:

```
+----------------------------------------------------+
| FILES TO COMPARE   (3 / 10)                        |
| 1.  essay_alice.txt          5210 chars   [Remove] |
| 2.  essay_bob.pdf            6041 chars   [Remove] |
|   +--------------------------------------------+   |
|   |          BUSY - please wait                |   |
|   |  OCR page 3/12 - handwritten.pdf ...       |   |
|   +--------------------------------------------+   |
+----------------------------------------------------+
```

For a handwritten / scanned PDF the banner text cycles per page
(`OCR page k/N - filename.pdf`); for native-text PDFs it just reads
`Extracting PDF: filename.pdf ...`.

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

### Pipeline (N documents)

```
each input file ─► loader:
                     .txt              -> read as-is
                     .pdf (native)     -> pdftotext
                     .pdf (handwritten)-> pdftoppm -r 200 -> Tesseract per page
                     .png/.jpg/...     -> Tesseract OCR
                ─► NormalizeText (lowercase + alphanumeric + collapse spaces)
                ─► stored as docs[i]

on Analyze:    for every pair (i, j) where i < j:
                   CalculateSimilarity (rolling-row DP)  ──► matrix[i][j]

               highest pair (a, b):
                   AnalyzeLCS (full DP) for the headline percentage

GDI render: N×N color-coded grid + top-pair headline + verdict
Save report: file index + verdict + ASCII similarity matrix
```

**Handwritten-PDF detection.** After `pdftotext` returns, the loader counts
non-whitespace bytes per page. Under ~30 chars/page → treat the PDF as
scanned/handwritten, free the empty text, and re-run via the OCR path. The
banner switches mid-load to `Handwritten PDF detected - running OCR on ...`
then to the per-page `OCR page k/N` status.

For 10 files that's 45 pair comparisons; each pair is `O(n*m)` in time but
only `O(min(n,m))` in memory, so analyzing 10 average essays takes a couple
of seconds and well under 1 MB of RAM during the matrix pass.

---

## Project layout

```
Plagiarism-Detector/
├── main.c             WinMain, window procedure, layout, analyze workflow
├── theme.h / .c       Color palette and font handles
├── painter.h / .c     GDI helpers: panels, progress bar, results, buttons
├── algorithm.h / .c   NormalizeText + CalculateSimilarity + AnalyzeLCS
├── file_picker.h / .c Win32 GetOpenFileName / GetSaveFileName wrappers
├── pdf_extract.h / .c pdftotext / pdfinfo / pdftoppm subprocess wrappers
│                      (incl. handwritten-PDF rasterize + per-page OCR loop)
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
| `pdftoppm`    | `C:\msys64\mingw64\bin\pdftoppm.exe`                |
| `pdfinfo`     | `C:\msys64\mingw64\bin\pdfinfo.exe`                 |

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
2. Click **Add Text / PDF** or **Add Image / OCR**. The Windows file dialog
   opens in multi-select mode:
   - **Ctrl-click** to toggle individual files into the selection.
   - **Shift-click** to select a contiguous range.
   - **Ctrl + A** to pick everything in the folder.
   - Click **Open** and every selected file loads in one shot.
3. Watch the BUSY banner cycle through each file as it loads (especially
   visible during OCR and PDF extraction, where each file can take seconds).
4. Use the per-row **Remove** button to drop any file from the list. The
   status bar at the bottom shows a running total like `Added 4 file(s).
   Total: 4/10`.
5. Click **ANALYZE PLAGIARISM** once you have at least 2 files. The button is
   disabled below that threshold.
6. Read the result: colored N×N matrix (rows + columns both labelled with
   `N. filename` and ellipsis-truncated to fit), the highest-pair headline,
   and the verdict band.
7. **Save Report** writes a `.txt` with the full ASCII matrix and details.
   **Reset** clears every file.

### Status bar quick reference

| Scenario                              | Status text                                                  |
|---------------------------------------|--------------------------------------------------------------|
| Multi-select all loaded cleanly       | `Added 4 file(s). Total: 4/10`                               |
| Hit the 10-file cap mid-batch         | `Added 6 file(s). 3 skipped (10-file cap). Total: 10/10`     |
| Some files failed to parse            | `Added 3 file(s). 1 failed to load. Total: 3/10`             |
| Analyze finished                      | `Analysis complete: 6 pairs, highest 78.2%`                  |

---

## Troubleshooting

| Symptom                                  | Cause                                                | Fix                                                                                  |
|------------------------------------------|------------------------------------------------------|--------------------------------------------------------------------------------------|
| Double-clicking exe → nothing happens    | MinGW DLLs not on PATH; `-mwindows` suppresses error | Launch via `run.bat`                                                                 |
| OCR returns empty text                   | `TESSDATA_PREFIX` not set or `eng.traineddata` missing | `set TESSDATA_PREFIX=C:\msys64\mingw64\share\tessdata`; reinstall `tesseract-data-eng` |
| PDF load fails or returns empty          | `pdftotext.exe` not on PATH                          | Install / reinstall `mingw-w64-x86_64-poppler`                                       |
| Handwritten PDF returns empty            | `pdftoppm.exe` missing or `%TEMP%` not writable      | Install `mingw-w64-x86_64-poppler`; verify `%TEMP%` exists and is writable           |
| Build fails: `tesseract/capi.h not found`| Tesseract dev headers missing                        | `pacman -S mingw-w64-x86_64-tesseract-ocr`                                           |
| Build fails: `gcc not found`             | MinGW64 not in shell PATH                            | Use the MSYS2 MinGW64 shell, or add `C:\msys64\mingw64\bin` to PATH                  |
| App opens but window appears blank/black | GDI font load failed (rare)                          | Verify Segoe UI is installed (default on Win 10/11)                                  |

For verbose DLL-load debugging, temporarily drop `-mwindows` from
`build.bat` to attach a console and surface real errors at startup.

---

## Implementation notes

- **No external UI framework.** All controls (buttons, panels, matrix grid)
  are owner-drawn via GDI in `painter.c`. The window procedure is in `main.c`.
- **Synchronous BUSY repaint.** OCR and PDF extraction run on the UI thread
  and would normally freeze the window. Before each slow load, `ShowBusy`
  invalidates the file-list rect *and* calls `UpdateWindow`, which pumps a
  WM_PAINT immediately — so the banner actually appears before the thread
  blocks, instead of after it returns.
- **Stay-responsive pump.** Between pages of a handwritten-PDF OCR run
  (and between files in a multi-select batch) the loader calls a
  `PumpMessages()` helper that drains the queue via `PeekMessage` — that
  is what keeps Windows from marking the window "Not responding" on long
  jobs. A `loading` flag in the `App` struct causes `WM_COMMAND` to drop
  any button clicks pumped during that window, so the user cannot trigger
  a re-entrant load or analyze pass on half-loaded state.
- **Handwritten-PDF pipeline.** `ExtractTextFromHandwrittenPDF` creates a
  unique subdirectory under `%TEMP%`, invokes
  `pdftoppm -r 200 -png "<pdf>" "<temp>\p"`, enumerates the resulting
  `p-*.png` files (lex-sorted, which equals page order because `pdftoppm`
  pads digits consistently within one job), runs Tesseract on each page,
  concatenates the pages with `\n\n` separators, and best-effort deletes
  the temp directory at the end. Pages that fail OCR are skipped rather
  than aborting the whole document.
- **Multi-select via callback.** The file picker uses
  `OFN_ALLOWMULTISELECT | OFN_EXPLORER` and parses both the single-file and
  directory-plus-NUL-separated-filenames buffer formats. Callers pass a
  `FilePickCallback` and can return `false` to stop the iteration early
  (used here to honour the 10-file cap mid-batch).
- **Pair-by-pair matrix.** `RunAnalysis` fills the N×N matrix with
  `CalculateSimilarity` (rolling-row DP, cheap memory) for every i&lt;j pair,
  then runs full-DP `AnalyzeLCS` **once** on the highest pair to recover
  the matched segment for display and the report.
- **Memory cap on full DP.** The full-table allocation is capped at roughly
  8 million cells (~64 MB at 8 bytes/cell). Pairs above this fall back to
  the rolling-row implementation, which still produces the percentage.
- **Normalization is on a copy.** Each document is normalized into a
  freshly allocated buffer; the original extracted text is preserved on
  the `DocState` for the report output.

---

## License

Educational / academic project. No license attached — open an issue if you
want to reuse it in a different context.
