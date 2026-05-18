#include "ocr_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tesseract/capi.h> // Tesseract C-API

static TessBaseAPI* handle = NULL;

bool InitializeOCR() {
    handle = TessBaseAPICreate();
    
    // Initialize tesseract-ocr with English, without specifying tessdata path.
    // It assumes TESSDATA_PREFIX environment variable is set or uses the default path.
    if (TessBaseAPIInit3(handle, NULL, "eng") != 0) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        return false;
    }
    return true;
}

void ShutdownOCR() {
    if (handle) {
        TessBaseAPIEnd(handle);
        TessBaseAPIDelete(handle);
        handle = NULL;
    }
}

char* ExtractTextFromImage(const char* imagePath) {
    if (!handle) {
        fprintf(stderr, "OCR Engine not initialized.\n");
        return NULL;
    }

    // Leptonica is normally used by Tesseract to read images, 
    // but in a pure capi sense without Leptonica headers available, 
    // we can use a trick with raylib or use simple file reading.
    // However, to keep it simple and within the C-API, Leptonica's pixRead is standard.
    // Assuming Leptonica's C-API is available as part of Tesseract installation:
    
    // As we might not have Leptonica headers easily included, we will use Tesseract's API 
    // that reads the image directly or Leptonica's pixRead from leptonica's allheaders.h
    // Let's declare pixRead manually to avoid complex header dependencies if leptonica isn't included easily.
    
    // Note: The standard way is using Leptonica. We assume leptonica headers are available if tesseract is.
    // If not, we might need a workaround. Let's include leptonica if possible.
    // For this boilerplate, we'll assume TessBaseAPIProcessPages is available which can read directly.

    // We can use TessBaseAPIProcessPages for direct file processing:
    // It processes the file and returns the text.
    // A simpler approach if Leptonica is missing is just using system calls or reading raw pixels.
    // Here we'll use TessBaseAPIProcessPages which reads an image file directly (requires Leptonica linked in Tesseract).

    // Tesseract 4/5 provides a way to process a file directly:
    // char* TessBaseAPIProcessPages(TessBaseAPI* handle, const char* filename, const char* retry_config, int timeout_millisec, struct ETEXT_DESC* monitor);
    
    // Using standard C-API to process pages. TessBaseAPIProcessPages returns a boolean (1 for success, 0 for failure)
    // However, it doesn't return the text directly. To get the text, we should call TessBaseAPIGetUTF8Text after setting the image.
    // If we don't have leptonica headers explicitly, we can still try to use ProcessPages.
    // But actually, ProcessPages is meant for writing output files. 
    // Let's use it and then call GetUTF8Text.
    
    int success = TessBaseAPIProcessPages(handle, imagePath, NULL, 0, NULL);
    
    if (success) {
        char* text = TessBaseAPIGetUTF8Text(handle);
        if (text) {
            // Copy to standard malloc'd string
            char* safeText = (char*)malloc(strlen(text) + 1);
            if (safeText) {
                strcpy(safeText, text);
            }
            TessDeleteText(text);
            return safeText;
        }
    }
    
    return NULL;
}
