#ifndef JOB_H
#define JOB_H

#include <stdio.h>
#include <stdlib.h>

#define READY 0
#define RUNNING 1
#define COMPLETED 2

#define MAX_JOBS 100
#define MAX_TIME_VALUE 100000
#define MAX_SLICES 4096
#define IDLE_ID (-1)

typedef struct {
  int id;
  int arrival;
  int burst;
  int priority;      // lower number = higher priority
  int remaining;
  int completion;
  int status;
  int totTime;       // turnaround time
  int waitTime;
} Job;

typedef struct Node {
  int index;
  struct Node* next;
} Node;

// one contiguous run of the CPU (id == IDLE_ID means the CPU was idle)
typedef struct {
  int id;
  int start;
  int end;
} Slice;

#endif
