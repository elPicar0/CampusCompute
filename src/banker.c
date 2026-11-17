#include <stdio.h>
#include <string.h>

#include "../include/campus.h"

#define NP 5     // processes
#define NR 3     // resource types (A, B, C)

typedef struct {
  int available[NR];
  int allocation[NP][NR];
  int maximum[NP][NR];
  int need[NP][NR];            // Need = Maximum - Allocation
} State;

static void printVec(const int* v) {
  printf("<");
  for (int r = 0; r < NR; r++) printf(r ? " %d" : "%d", v[r]);
  printf(">");
}

// validates the matrices and fills Need; returns 0 if the state is usable
static int computeNeed(State* s) {
  for (int r = 0; r < NR; r++) {
    if (s->available[r] < 0) {
      fprintf(stderr, "error: Available is negative for resource %c\n", 'A' + r);
      return -1;
    }
  }
  for (int i = 0; i < NP; i++) {
    for (int r = 0; r < NR; r++) {
      if (s->allocation[i][r] < 0 || s->maximum[i][r] < 0) {
        fprintf(stderr, "error: negative value for P%d resource %c\n", i + 1, 'A' + r);
        return -1;
      }
      if (s->allocation[i][r] > s->maximum[i][r]) {
        fprintf(stderr, "error: P%d holds more of %c than its Maximum\n", i + 1, 'A' + r);
        return -1;
      }
      s->need[i][r] = s->maximum[i][r] - s->allocation[i][r];
    }
  }
  return 0;
}

static void printState(const State* s) {
  printf("Available: ");
  printVec(s->available);
  printf("\n%-5s %-14s %-14s %-14s\n", "", "Allocation", "Maximum", "Need");
  for (int i = 0; i < NP; i++) {
    printf("P%-4d ", i + 1);
    const int* rows[3] = {s->allocation[i], s->maximum[i], s->need[i]};
    for (int k = 0; k < 3; k++) {
      char buf[32];
      int len = snprintf(buf, sizeof(buf), "<");
      for (int r = 0; r < NR; r++)
        len += snprintf(buf + len, sizeof(buf) - (size_t)len, r ? " %d" : "%d", rows[k][r]);
      snprintf(buf + len, sizeof(buf) - (size_t)len, ">");
      printf("%-14s ", buf);
    }
    printf("\n");
  }
}

// safety algorithm: Work = Available; repeatedly let any process whose Need
// fits in Work finish and release its Allocation. Safe if everyone finishes.
static int isSafe(const State* s, int seq[NP]) {
  int work[NR];
  int finish[NP] = {0};
  int count = 0;
  memcpy(work, s->available, sizeof(work));

  int progress = 1;
  while (progress && count < NP) {
    progress = 0;
    for (int i = 0; i < NP; i++) {
      if (finish[i]) continue;

      int fits = 1;
      for (int r = 0; r < NR; r++)
        if (s->need[i][r] > work[r]) { fits = 0; break; }
      if (!fits) continue;

      printf("  P%d can finish: Need ", i + 1);
      printVec(s->need[i]);
      printf(" <= Work ");
      printVec(work);
      for (int r = 0; r < NR; r++) work[r] += s->allocation[i][r];   // P_i releases everything
      printf("  -> Work ");
      printVec(work);
      printf("\n");

      finish[i] = 1;
      seq[count++] = i;
      progress = 1;
    }
  }

  if (count < NP) {
    printf("  No remaining process can finish with Work ");
    printVec(work);
    printf(". Blocked:\n");
    for (int i = 0; i < NP; i++) {
      if (finish[i]) continue;
      printf("    P%d needs ", i + 1);
      printVec(s->need[i]);
      printf(" which exceeds Work\n");
    }
  }
  return count == NP;
}

// returns 1 safe, 0 unsafe, -1 invalid state
static int analyze(State* s, const char* label) {
  printf("\n=== Banker's safety algorithm: %s ===\n", label);
  if (computeNeed(s) != 0) return -1;
  printState(s);
  printf("Safety check (Work starts as Available):\n");

  int seq[NP];
  int safe = isSafe(s, seq);
  if (safe) {
    printf("Result: SAFE state. Safe sequence: ");
    for (int i = 0; i < NP; i++) printf(i ? " -> P%d" : "P%d", seq[i] + 1);
    printf("\n");
  } else {
    printf("Result: UNSAFE state. No safe sequence exists; granting these requests could lead to deadlock.\n");
  }
  return safe;
}

int banker_algo_success(void) {
  State s = {
    .available = {3, 3, 2},
    .allocation = {
      //A B C
      {0, 1, 0},
      {2, 0, 0},
      {3, 0, 2},
      {2, 1, 1},
      {0, 0, 2}
    },
    .maximum = {
      {7, 5, 3},
      {3, 2, 2},
      {9, 0, 2},
      {4, 2, 2},
      {5, 3, 2}
    }
  };
  return analyze(&s, "SAFE case") == 1 ? 0 : 1;
}

int banker_algo_deadlock(void) {
  State s = {
    .available = {1, 0, 0},
    .allocation = {
      {0, 1, 0},
      {2, 0, 0},
      {3, 0, 2},
      {2, 1, 1},
      {0, 0, 2}
    },
    .maximum = {
      {7, 5, 3},
      {3, 2, 2},
      {9, 0, 2},
      {4, 2, 2},
      {5, 3, 3}
    }
  };
  return analyze(&s, "UNSAFE case") == 0 ? 0 : 1;
}

int main(void) {
  printf("|------FR5------|\n");
  int rc = banker_algo_success();
  rc |= banker_algo_deadlock();
  return rc;
}
