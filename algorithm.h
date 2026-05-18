#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <stddef.h>

// Normalize text by removing punctuation, lowercasing, and collapsing whitespace.
// Caller must ensure dest has space for at least strlen(src) + 1 bytes.
void NormalizeText(char* dest, const char* src);

// Calculate similarity percentage between two normalized texts using LCS DP.
double CalculateSimilarity(const char* text1, const char* text2);

// Detailed LCS analysis. Fills out_lcs_len and writes a recovered LCS string into
// out_lcs (malloc'd, caller must free) if non-NULL. Returns similarity percent.
// Texts must already be normalized.
double AnalyzeLCS(const char* text1, const char* text2,
                  int* out_lcs_len, char** out_lcs);

#endif // ALGORITHM_H
