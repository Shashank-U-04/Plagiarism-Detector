#ifndef OCR_ENGINE_H
#define OCR_ENGINE_H

#include <stdbool.h>

// Initializes the Tesseract OCR engine.
// Returns true if successful, false otherwise.
bool InitializeOCR();

// Closes the Tesseract engine and frees resources.
void ShutdownOCR();

// Extracts text from the given image file.
// Returns a dynamically allocated string containing the text. The caller must free() it.
// Returns NULL on failure.
char* ExtractTextFromImage(const char* imagePath);

#endif // OCR_ENGINE_H
