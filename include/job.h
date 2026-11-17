#ifndef JOB_H
#define JOB_H

#include <stddef.h>

#define MAX_JOBS 64
#define MAX_TIME_VALUE 100000

/* Process lifecycle of a simulated job (FR1). */
typedef enum { JOB_NEW, JOB_READY, JOB_RUNNING, JOB_DONE } JobStatus;

typedef struct {
  int jobID;
  int arrivalTime;
  int burstTime;
  int priority;        /* lower number = higher priority */
  JobStatus status;
  int remaining;       /* used by Round Robin */
  int completion;      /* completion time (CT) */
  int tat;             /* turnaround = CT - arrival */
  int wt;              /* waiting    = TAT - burst */
} Job;

const char* jobStatusName(JobStatus s);

/* Return 0 if valid, -1 otherwise (reason copied into err). */
int validateJob(const Job* j, char* err, size_t errLen);
int validateJobSet(const Job* jobs, int n, char* err, size_t errLen);

#endif
