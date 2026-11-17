#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/* Strict integer parse: whole string must be a number within [min, max].
 * Returns 0 on success, -1 on any error. */
int parseInt(const char* s, long min, long max, int* out);

/* Prints "=== title ===" style section banner. */
void printBanner(const char* title);

/* mkdir if missing. Returns 0 if the directory exists afterwards. */
int ensureDir(const char* path);

/* Reads whitespace separated integers from a file ('#' starts a comment).
 * Fills buf (up to max values) and sets *count. Returns 0 or -1 (message
 * printed to stderr on error). */
int readIntFile(const char* path, int* buf, int max, int* count);

/* Number of lines in a text file, or -1 if it cannot be opened. */
int countLines(const char* path);

#endif
