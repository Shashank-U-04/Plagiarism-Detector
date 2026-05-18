#ifndef FILE_PICKER_H
#define FILE_PICKER_H

#include <stdbool.h>

// Open a native Win32 file picker accepting PDF, image, or text files.
// On success fills outPath (size outSize) and returns true.
bool PickInputFile(char* outPath, int outSize);

// Pick a document (.txt or .pdf). Returns true on success.
bool PickDocumentFile(void* hwndOwner, char* outPath, int outSize);

// Pick an image for OCR (.png/.jpg/.jpeg/.bmp/.tif/.tiff). Returns true on success.
bool PickImageFile(void* hwndOwner, char* outPath, int outSize);

// Save-as dialog for report (.txt). Returns true on success.
bool PickSaveReportFile(void* hwndOwner, char* outPath, int outSize);

#endif // FILE_PICKER_H
