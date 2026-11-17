#include "../include/job.h"

#include <stdio.h>

const char* jobStatusName(JobStatus s) {
  switch (s) {
    case JOB_NEW:     return "NEW";
    case JOB_READY:   return "READY";
    case JOB_RUNNING: return "RUNNING";
    case JOB_DONE:    return "DONE";
  }
  return "?";
}

int validateJob(const Job* j, char* err, size_t errLen) {
  if (j->jobID <= 0)
    snprintf(err, errLen, "job ID must be positive (got %d)", j->jobID);
  else if (j->arrivalTime < 0 || j->arrivalTime > MAX_TIME_VALUE)
    snprintf(err, errLen, "J%d: arrival time must be 0..%d (got %d)", j->jobID, MAX_TIME_VALUE, j->arrivalTime);
  else if (j->burstTime <= 0 || j->burstTime > MAX_TIME_VALUE)
    snprintf(err, errLen, "J%d: CPU burst must be 1..%d (got %d)", j->jobID, MAX_TIME_VALUE, j->burstTime);
  else if (j->priority < 0 || j->priority > MAX_TIME_VALUE)
    snprintf(err, errLen, "J%d: priority must be 0..%d (got %d)", j->jobID, MAX_TIME_VALUE, j->priority);
  else
    return 0;
  return -1;
}

int validateJobSet(const Job* jobs, int n, char* err, size_t errLen) {
  if (n <= 0) {
    snprintf(err, errLen, "no jobs to schedule");
    return -1;
  }
  if (n > MAX_JOBS) {
    snprintf(err, errLen, "too many jobs (limit %d)", MAX_JOBS);
    return -1;
  }
  for (int i = 0; i < n; i++) {
    if (validateJob(&jobs[i], err, errLen) != 0) return -1;
    for (int k = 0; k < i; k++) {
      if (jobs[k].jobID == jobs[i].jobID) {
        snprintf(err, errLen, "duplicate job ID %d", jobs[i].jobID);
        return -1;
      }
    }
  }
  return 0;
}
