#include "../include/banker.h"

#include <stdio.h>
#include <string.h>

#include "../include/util.h"

int bankerComputeNeed(BankerState* s, char* err, size_t errLen) {
  if (s->n < 1 || s->n > BANKER_MAX_P || s->m < 1 || s->m > BANKER_MAX_R) {
    snprintf(err, errLen, "need 1..%d processes and 1..%d resource types", BANKER_MAX_P, BANKER_MAX_R);
    return -1;
  }
  for (int r = 0; r < s->m; r++) {
    if (s->available[r] < 0) {
      snprintf(err, errLen, "Available[R%d] is negative", r);
      return -1;
    }
  }
  for (int i = 0; i < s->n; i++) {
    for (int r = 0; r < s->m; r++) {
      if (s->allocation[i][r] < 0 || s->maximum[i][r] < 0) {
        snprintf(err, errLen, "negative value for P%d R%d", i, r);
        return -1;
      }
      if (s->allocation[i][r] > s->maximum[i][r]) {
        snprintf(err, errLen, "P%d holds more of R%d (%d) than its Maximum (%d)", i, r,
                 s->allocation[i][r], s->maximum[i][r]);
        return -1;
      }
      s->need[i][r] = s->maximum[i][r] - s->allocation[i][r];
    }
  }
  return 0;
}

static void printVec(const int* v, int m) {
  printf("<");
  for (int r = 0; r < m; r++) printf(r ? " %d" : "%d", v[r]);
  printf(">");
}

int bankerIsSafe(const BankerState* s, int* seq, int verbose) {
  int work[BANKER_MAX_R];
  int finish[BANKER_MAX_P] = {0};
  int count = 0;
  memcpy(work, s->available, sizeof(int) * (size_t)s->m);

  int progress = 1;
  while (progress && count < s->n) {
    progress = 0;
    for (int i = 0; i < s->n; i++) {
      if (finish[i]) continue;
      int ok = 1;
      for (int r = 0; r < s->m; r++)
        if (s->need[i][r] > work[r]) { ok = 0; break; }
      if (!ok) continue;

      if (verbose) {
        printf("  P%d can finish: Need ", i);
        printVec(s->need[i], s->m);
        printf(" <= Work ");
        printVec(work, s->m);
      }
      for (int r = 0; r < s->m; r++) work[r] += s->allocation[i][r];   /* P_i releases everything */
      finish[i] = 1;
      seq[count++] = i;
      progress = 1;
      if (verbose) {
        printf("  -> Work ");
        printVec(work, s->m);
        printf("\n");
      }
    }
  }

  if (verbose && count < s->n) {
    printf("  No remaining process can finish with Work ");
    printVec(work, s->m);
    printf(". Blocked processes:\n");
    for (int i = 0; i < s->n; i++) {
      if (finish[i]) continue;
      printf("    P%d needs ", i);
      printVec(s->need[i], s->m);
      printf(" which exceeds Work\n");
    }
  }
  return count == s->n;
}

void bankerPrintState(const BankerState* s) {
  printf("Processes: %d, resource types: %d\n", s->n, s->m);
  printf("Available: ");
  printVec(s->available, s->m);
  printf("\n%-5s %-14s %-14s %-14s\n", "", "Allocation", "Maximum", "Need");
  for (int i = 0; i < s->n; i++) {
    printf("P%-4d ", i);
    char buf[48];
    const int* rows[3] = {s->allocation[i], s->maximum[i], s->need[i]};
    for (int k = 0; k < 3; k++) {
      int len = 0;
      len += snprintf(buf + len, sizeof(buf) - (size_t)len, "<");
      for (int r = 0; r < s->m; r++)
        len += snprintf(buf + len, sizeof(buf) - (size_t)len, r ? " %d" : "%d", rows[k][r]);
      snprintf(buf + len, sizeof(buf) - (size_t)len, ">");
      printf("%-14s ", buf);
    }
    printf("\n");
  }
}

int bankerLoadFile(const char* path, BankerState* s) {
  int vals[2 + BANKER_MAX_R + 2 * BANKER_MAX_P * BANKER_MAX_R];
  int count = 0;
  if (readIntFile(path, vals, (int)(sizeof(vals) / sizeof(vals[0])), &count) != 0) return -1;
  if (count < 2) {
    fprintf(stderr, "error: %s: expected 'n m' on the first line\n", path);
    return -1;
  }
  memset(s, 0, sizeof(*s));
  s->n = vals[0];
  s->m = vals[1];
  if (s->n < 1 || s->n > BANKER_MAX_P || s->m < 1 || s->m > BANKER_MAX_R) {
    fprintf(stderr, "error: %s: need 1..%d processes and 1..%d resource types\n", path, BANKER_MAX_P, BANKER_MAX_R);
    return -1;
  }
  int expected = 2 + s->m + 2 * s->n * s->m;
  if (count != expected) {
    fprintf(stderr, "error: %s: expected %d integers (n m, Available, Allocation, Maximum) but found %d\n",
            path, expected, count);
    return -1;
  }
  int k = 2;
  for (int r = 0; r < s->m; r++) s->available[r] = vals[k++];
  for (int i = 0; i < s->n; i++)
    for (int r = 0; r < s->m; r++) s->allocation[i][r] = vals[k++];
  for (int i = 0; i < s->n; i++)
    for (int r = 0; r < s->m; r++) s->maximum[i][r] = vals[k++];
  return 0;
}

/* Classic textbook state: SAFE, sequence P1 P3 P4 P0 P2. */
void bankerSampleSafe(BankerState* s) {
  static const int alloc[5][3] = {{0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}};
  static const int max[5][3] = {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}};
  memset(s, 0, sizeof(*s));
  s->n = 5;
  s->m = 3;
  s->available[0] = 3; s->available[1] = 3; s->available[2] = 2;
  for (int i = 0; i < 5; i++)
    for (int r = 0; r < 3; r++) {
      s->allocation[i][r] = alloc[i][r];
      s->maximum[i][r] = max[i][r];
    }
}

/* Same processes, but almost nothing available: UNSAFE. */
void bankerSampleUnsafe(BankerState* s) {
  bankerSampleSafe(s);
  s->available[0] = 0; s->available[1] = 1; s->available[2] = 1;
}

int bankerAnalyze(BankerState* s, const char* label, Logger* lg) {
  char err[160];
  printf("\n=== Banker's safety algorithm: %s ===\n", label);
  if (bankerComputeNeed(s, err, sizeof(err)) != 0) {
    fprintf(stderr, "error: invalid state: %s\n", err);
    return -1;
  }
  bankerPrintState(s);
  printf("Safety check (Work starts as Available):\n");

  int seq[BANKER_MAX_P];
  int safe = bankerIsSafe(s, seq, 1);
  if (safe) {
    printf("Result: SAFE state. Safe sequence: ");
    for (int i = 0; i < s->n; i++) printf(i ? " -> P%d" : "P%d", seq[i]);
    printf("\n");
    loggerLog(lg, "BANKER", "%s: SAFE, first process in safe sequence is P%d", label, seq[0]);
  } else {
    printf("Result: UNSAFE state. No safe sequence exists; granting these requests could deadlock.\n");
    loggerLog(lg, "BANKER", "%s: UNSAFE, no safe sequence", label);
  }
  return safe;
}
