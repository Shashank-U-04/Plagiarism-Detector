#ifndef PDF_EXTRACT_H
#define PDF_EXTRACT_H

// Extract all text from a PDF using pdftotext.exe (poppler).
// Returns a malloc'd, null-terminated string containing concatenated text
// from every page. Caller must free(). Returns NULL on failure.
char* ExtractTextFromPDF(const char* pdfPath);

// Return the page count of a PDF using pdfinfo.exe. Returns 0 on failure.
int GetPDFPageCount(const char* pdfPath);

// Per-page progress callback fired during handwritten-PDF OCR. `page` is
// 1-based, `total` is the page count. Safe to update UI here (callback runs
// on the calling thread, between page OCR passes).
typedef void (*PdfOcrProgressFn)(int page, int total, void* user);

// Rasterize a PDF with pdftoppm and run Tesseract OCR on each page image.
// Caller must have initialized the OCR engine first (InitializeOCR()).
// Returns a malloc'd, null-terminated string with all pages concatenated
// (pages separated by "\n\n"). Caller must free(). Returns NULL on failure.
char* ExtractTextFromHandwrittenPDF(const char* pdfPath,
                                    PdfOcrProgressFn progress,
                                    void* user);

#endif // PDF_EXTRACT_H
