#ifndef FILE_PICKER_H
#define FILE_PICKER_H

#include <stdbool.h>

// Callback invoked once per chosen path. Return false to stop the iteration
// early (e.g. when the caller's capacity is full).
typedef bool (*FilePickCallback)(const char* fullPath, void* user);

// Multi-select document picker (.txt / .pdf). Returns number of files
// successfully visited; 0 if the user cancelled.
int PickDocumentFiles(void* hwndOwner, FilePickCallback visit, void* user);

// Multi-select image picker for OCR (.png/.jpg/.jpeg/.bmp/.tif/.tiff).
int PickImageFiles(void* hwndOwner, FilePickCallback visit, void* user);

// Save-as dialog for report (.txt). Returns true on success.
bool PickSaveReportFile(void* hwndOwner, char* outPath, int outSize);

#endif // FILE_PICKER_H
