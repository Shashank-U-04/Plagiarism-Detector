#include "algorithm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void NormalizeText(char* dest, const char* src) {
    int j = 0;
    for (int i = 0; src[i] != '\0'; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || isspace(c)) {
            if (isspace(c)) {
                if (j > 0 && dest[j - 1] != ' ') {
                    dest[j++] = ' ';
                }
            } else {
                dest[j++] = (char)tolower(c);
            }
        }
    }
    if (j > 0 && dest[j - 1] == ' ') j--;
    dest[j] = '\0';
}

static int imax(int a, int b) { return a > b ? a : b; }

double CalculateSimilarity(const char* text1, const char* text2) {
    int len1 = (int)strlen(text1);
    int len2 = (int)strlen(text2);

    if (len1 == 0 && len2 == 0) return 100.0;
    if (len1 == 0 || len2 == 0) return 0.0;

    int* prev = (int*)calloc(len2 + 1, sizeof(int));
    int* curr = (int*)calloc(len2 + 1, sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); return 0.0; }

    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            if (text1[i - 1] == text2[j - 1]) {
                curr[j] = prev[j - 1] + 1;
            } else {
                curr[j] = imax(prev[j], curr[j - 1]);
            }
        }
        int* t = prev; prev = curr; curr = t;
    }

    int lcs_len = prev[len2];
    free(prev); free(curr);
    return ((double)lcs_len / imax(len1, len2)) * 100.0;
}

// Full O(n*m) DP table for LCS recovery. Used only on the analyze button press
// and bounded by user-supplied input — fine for documents up to a few thousand chars.
double AnalyzeLCS(const char* text1, const char* text2,
                  int* out_lcs_len, char** out_lcs) {
    int len1 = (int)strlen(text1);
    int len2 = (int)strlen(text2);

    if (out_lcs_len) *out_lcs_len = 0;
    if (out_lcs) *out_lcs = NULL;

    if (len1 == 0 && len2 == 0) return 100.0;
    if (len1 == 0 || len2 == 0) return 0.0;

    // Guard against huge inputs — fall back to memory-light percentage only.
    const long long MAX_CELLS = 8LL * 1024 * 1024; // ~32 MB at 4 bytes/cell
    if ((long long)(len1 + 1) * (long long)(len2 + 1) > MAX_CELLS) {
        double pct = CalculateSimilarity(text1, text2);
        return pct;
    }

    int rows = len1 + 1;
    int cols = len2 + 1;
    int* dp = (int*)calloc((size_t)rows * cols, sizeof(int));
    if (!dp) return CalculateSimilarity(text1, text2);

    #define DP(i,j) dp[(i) * cols + (j)]
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            if (text1[i - 1] == text2[j - 1]) {
                DP(i,j) = DP(i-1,j-1) + 1;
            } else {
                DP(i,j) = imax(DP(i-1,j), DP(i,j-1));
            }
        }
    }
    int lcs_len = DP(len1, len2);

    char* lcs = (char*)malloc((size_t)lcs_len + 1);
    if (lcs) {
        int i = len1, j = len2, k = lcs_len;
        lcs[lcs_len] = '\0';
        while (i > 0 && j > 0 && k > 0) {
            if (text1[i - 1] == text2[j - 1]) {
                lcs[--k] = text1[i - 1];
                i--; j--;
            } else if (DP(i-1,j) >= DP(i,j-1)) {
                i--;
            } else {
                j--;
            }
        }
    }
    #undef DP
    free(dp);

    if (out_lcs_len) *out_lcs_len = lcs_len;
    if (out_lcs) *out_lcs = lcs;
    else free(lcs);

    return ((double)lcs_len / imax(len1, len2)) * 100.0;
}
