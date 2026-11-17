#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "job.h"

#define IDLE_ID (-1)
#define DEFAULT_QUANTUM 2

typedef enum { POLICY_FCFS, POLICY_RR, POLICY_PRIORITY } Policy;

/* One contiguous run of the CPU (jobID == IDLE_ID means the CPU was idle). */
typedef struct { int jobID; int start; int end; } Slice;

typedef struct {
  Slice* slices;
  int count;
  int cap;
} Timeline;

typedef struct { double avgCT, avgTAT, avgWT; } Metrics;

void timelineInit(Timeline* t);
void timelineFree(Timeline* t);
int timelineAdd(Timeline* t, int jobID, int start, int end);

/* Core algorithms. They sort/modify the array they are given (pass a copy)
 * and return 0 on success, -1 on error (e.g. out of memory). */
int fcfs(Job* j, int n, Timeline* t);
int roundRobin(Job* j, int n, int quantum, Timeline* t);
int priorityScheduling(Job* j, int n, Timeline* t);

const char* policyName(Policy p);
const char* policyTieRule(Policy p);

/* Copies `in` to `out`, runs the policy. */
int simulate(Policy p, const Job* in, int n, int quantum, Job* out, Timeline* t);
Metrics computeMetrics(const Job* jobs, int n);

void printTimeline(const Timeline* t);
void printMetricsTable(const Job* jobs, int n, const Metrics* m);

/* Full report for one policy (header, rules, timeline, metrics). */
int reportPolicy(Policy p, const Job* jobs, int n, int quantum);
/* Same job set under all three policies, side by side. */
int comparePolicies(const Job* jobs, int n, int quantum);

#endif
