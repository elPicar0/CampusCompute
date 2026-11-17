#include "../include/jobio.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int isBlank(const char* s) {
  while (*s != '\0') {
    if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') return 0;
    s++;
  }
  return 1;
}

int loadJobs(const char* path, Job* jobs, int max) {
  FILE* fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
    return -1;
  }

  char line[256], err[160];
  int n = 0, lineNo = 0;

  while (fgets(line, sizeof(line), fp) != NULL) {
    lineNo++;
    char* hash = strchr(line, '#');
    if (hash != NULL) *hash = '\0';
    if (isBlank(line)) continue;

    if (n >= max) {
      fprintf(stderr, "error: %s line %d: too many jobs (limit %d)\n", path, lineNo, max);
      fclose(fp);
      return -1;
    }

    long id, arr, burst, prio;
    int used = 0;
    int got = sscanf(line, "%ld %ld %ld %ld %n", &id, &arr, &burst, &prio, &used);
    if (got != 4) {
      fprintf(stderr, "error: %s line %d: expected 'jobID arrival burst priority' (4 integers)\n", path, lineNo);
      fclose(fp);
      return -1;
    }
    if (!isBlank(line + used)) {
      fprintf(stderr, "error: %s line %d: unexpected trailing text '%s'\n", path, lineNo, line + used);
      fclose(fp);
      return -1;
    }
    if (id < 0 || id > MAX_TIME_VALUE || arr < -1 || arr > MAX_TIME_VALUE ||
        burst < -1 || burst > MAX_TIME_VALUE || prio < -1 || prio > MAX_TIME_VALUE) {
      fprintf(stderr, "error: %s line %d: value out of range\n", path, lineNo);
      fclose(fp);
      return -1;
    }

    Job j = {.jobID = (int)id, .arrivalTime = (int)arr, .burstTime = (int)burst,
             .priority = (int)prio, .status = JOB_NEW, .remaining = (int)burst};
    if (validateJob(&j, err, sizeof(err)) != 0) {
      fprintf(stderr, "error: %s line %d: %s\n", path, lineNo, err);
      fclose(fp);
      return -1;
    }
    jobs[n++] = j;
  }
  fclose(fp);

  if (validateJobSet(jobs, n, err, sizeof(err)) != 0) {
    fprintf(stderr, "error: %s: %s\n", path, err);
    return -1;
  }
  return n;
}

int defaultJobs(Job* jobs, int max) {
  /* id arrival burst priority. J2 and J5 share priority 1 (tie-break demo). */
  static const int data[][4] = {
    {1, 0, 5, 3}, {2, 1, 3, 1}, {3, 2, 8, 4}, {4, 3, 6, 2}, {5, 4, 2, 1},
  };
  int n = (int)(sizeof(data) / sizeof(data[0]));
  if (n > max) n = max;
  for (int i = 0; i < n; i++) {
    jobs[i] = (Job){.jobID = data[i][0], .arrivalTime = data[i][1], .burstTime = data[i][2],
                    .priority = data[i][3], .status = JOB_NEW, .remaining = data[i][2]};
  }
  return n;
}

void printJobTable(const Job* jobs, int n) {
  printf("%-6s %-8s %-6s %-8s %-8s\n", "Job", "Arrival", "Burst", "Priority", "Status");
  for (int i = 0; i < n; i++) {
    printf("J%-5d %-8d %-6d %-8d %-8s\n", jobs[i].jobID, jobs[i].arrivalTime,
           jobs[i].burstTime, jobs[i].priority, jobStatusName(jobs[i].status));
  }
}
