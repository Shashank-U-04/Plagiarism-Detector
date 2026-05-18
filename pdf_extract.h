#ifndef PDF_EXTRACT_H
#define PDF_EXTRACT_H

// Extract all text from a PDF using pdftotext.exe (poppler).
// Returns a malloc'd, null-terminated string containing concatenated text
// from every page. Caller must free(). Returns NULL on failure.
char* ExtractTextFromPDF(const char* pdfPath);

// Return the page count of a PDF using pdfinfo.exe. Returns 0 on failure.
int GetPDFPageCount(const char* pdfPath);

#endif // PDF_EXTRACT_H
