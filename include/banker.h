#ifndef BANKER_H
#define BANKER_H

#include <stddef.h>

#include "logger.h"

#define BANKER_MAX_P 10
#define BANKER_MAX_R 10

typedef struct {
  int n, m;                                    /* processes, resource types */
  int available[BANKER_MAX_R];
  int allocation[BANKER_MAX_P][BANKER_MAX_R];
  int maximum[BANKER_MAX_P][BANKER_MAX_R];
  int need[BANKER_MAX_P][BANKER_MAX_R];        /* Need = Maximum - Allocation */
} BankerState;

/* Validates the matrices (non-negative, Allocation <= Maximum) and fills
 * Need. Returns 0 or -1 (reason in err). */
int bankerComputeNeed(BankerState* s, char* err, size_t errLen);

/* Safety algorithm. Returns 1 if safe (seq holds s->n process indices),
 * 0 if unsafe. verbose prints every step. */
int bankerIsSafe(const BankerState* s, int* seq, int verbose);

void bankerPrintState(const BankerState* s);

/* File format (integers, '#' comments):
 *   n m
 *   Available (m values)
 *   Allocation (n rows of m values)
 *   Maximum    (n rows of m values) */
int bankerLoadFile(const char* path, BankerState* s);
void bankerSampleSafe(BankerState* s);
void bankerSampleUnsafe(BankerState* s);

/* Prints the state and the verdict. Returns 1 safe, 0 unsafe, -1 on error. */
int bankerAnalyze(BankerState* s, const char* label, Logger* lg);

#endif
