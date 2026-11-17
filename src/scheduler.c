#include "../include/scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/queue.h"

/* ---------- timeline ---------- */

void timelineInit(Timeline* t) {
  t->slices = NULL;
  t->count = 0;
  t->cap = 0;
}

void timelineFree(Timeline* t) {
  free(t->slices);
  timelineInit(t);
}

int timelineAdd(Timeline* t, int jobID, int start, int end) {
  if (t->count == t->cap) {
    int newCap = t->cap == 0 ? 32 : t->cap * 2;
    Slice* p = realloc(t->slices, (size_t)newCap * sizeof(Slice));
    if (p == NULL) return -1;
    t->slices = p;
    t->cap = newCap;
  }
  t->slices[t->count++] = (Slice){jobID, start, end};
  return 0;
}

/* ---------- comparators ---------- */

/* Earlier arrival first; ties -> lower job ID first. */
static int byArrival(const void* j1, const void* j2) {
  const Job* t1 = j1;
  const Job* t2 = j2;
  if (t1->arrivalTime != t2->arrivalTime)
    return (t1->arrivalTime > t2->arrivalTime) - (t1->arrivalTime < t2->arrivalTime);
  return (t1->jobID > t2->jobID) - (t1->jobID < t2->jobID);
}

static int byId(const void* j1, const void* j2) {
  const Job* t1 = j1;
  const Job* t2 = j2;
  return (t1->jobID > t2->jobID) - (t1->jobID < t2->jobID);
}

static void finishJob(Job* j, int time) {
  j->completion = time;
  j->tat = time - j->arrivalTime;
  j->wt = j->tat - j->burstTime;
  j->status = JOB_DONE;
}

/* ---------- FCFS ---------- */

int fcfs(Job* j, int n, Timeline* t) {
  qsort(j, n, sizeof(Job), byArrival);
  int time = 0;
  for (int i = 0; i < n; i++) {
    if (time < j[i].arrivalTime) {                  /* CPU idle until arrival */
      if (timelineAdd(t, IDLE_ID, time, j[i].arrivalTime) != 0) return -1;
      time = j[i].arrivalTime;
    }
    j[i].status = JOB_RUNNING;
    if (timelineAdd(t, j[i].jobID, time, time + j[i].burstTime) != 0) return -1;
    time += j[i].burstTime;
    finishJob(&j[i], time);
  }
  return 0;
}

/* ---------- Round Robin ---------- */

int roundRobin(Job* j, int n, int quantum, Timeline* t) {
  if (quantum <= 0 || n <= 0) return -1;
  Queue* q = initQueue();
  if (q == NULL) return -1;
  int time = 0, done = 0, next = 0;                 /* next = first job not yet enqueued */

  qsort(j, n, sizeof(Job), byArrival);
  for (int i = 0; i < n; i++) {
    j[i].remaining = j[i].burstTime;
    j[i].status = JOB_NEW;
  }

  while (done < n) {
    while (next < n && j[next].arrivalTime <= time) {
      j[next].status = JOB_READY;
      enqueue(&q, next++);
    }

    if (isEmpty(q)) {                               /* idle: jump to next arrival */
      if (timelineAdd(t, IDLE_ID, time, j[next].arrivalTime) != 0) { freeQueue(&q); return -1; }
      time = j[next].arrivalTime;
      continue;
    }

    int cur = dequeue(&q);
    int slice = j[cur].remaining < quantum ? j[cur].remaining : quantum;
    j[cur].status = JOB_RUNNING;
    if (timelineAdd(t, j[cur].jobID, time, time + slice) != 0) { freeQueue(&q); return -1; }
    time += slice;
    j[cur].remaining -= slice;

    /* jobs that arrived DURING this slice are queued before the preempted job */
    while (next < n && j[next].arrivalTime <= time) {
      j[next].status = JOB_READY;
      enqueue(&q, next++);
    }

    if (j[cur].remaining > 0) {
      j[cur].status = JOB_READY;
      enqueue(&q, cur);
    } else {
      finishJob(&j[cur], time);
      done++;
    }
  }
  freeQueue(&q);
  return 0;
}

/* ---------- Non-preemptive Priority ---------- */

/* Lower priority number = higher priority.
 * Ties: earlier arrival, then lower job ID (array is sorted by arrival, ID,
 * and a strict '<' keeps the earlier index). */
int priorityScheduling(Job* j, int n, Timeline* t) {
  if (n <= 0) return -1;
  qsort(j, n, sizeof(Job), byArrival);
  for (int i = 0; i < n; i++) j[i].status = JOB_NEW;

  int time = 0, done = 0;
  while (done < n) {
    int best = -1;
    for (int i = 0; i < n; i++) {
      if (j[i].status == JOB_DONE || j[i].arrivalTime > time) continue;
      if (best == -1 || j[i].priority < j[best].priority) best = i;
    }

    if (best == -1) {                               /* nothing ready: idle */
      int nextArr = -1;
      for (int i = 0; i < n; i++)
        if (j[i].status != JOB_DONE && (nextArr == -1 || j[i].arrivalTime < nextArr))
          nextArr = j[i].arrivalTime;
      if (timelineAdd(t, IDLE_ID, time, nextArr) != 0) return -1;
      time = nextArr;
      continue;
    }

    j[best].status = JOB_RUNNING;
    if (timelineAdd(t, j[best].jobID, time, time + j[best].burstTime) != 0) return -1;
    time += j[best].burstTime;                      /* non-preemptive: runs to completion */
    finishJob(&j[best], time);
    done++;
  }
  return 0;
}

/* ---------- driver helpers ---------- */

const char* policyName(Policy p) {
  switch (p) {
    case POLICY_FCFS:     return "FCFS";
    case POLICY_RR:       return "Round Robin";
    case POLICY_PRIORITY: return "Priority (non-preemptive)";
  }
  return "?";
}

const char* policyTieRule(Policy p) {
  switch (p) {
    case POLICY_FCFS:
      return "ties on arrival time are broken by lower job ID";
    case POLICY_RR:
      return "ties on arrival -> lower job ID first; jobs arriving during a slice queue before the preempted job";
    case POLICY_PRIORITY:
      return "lower number = higher priority; ties -> earlier arrival, then lower job ID";
  }
  return "";
}

int simulate(Policy p, const Job* in, int n, int quantum, Job* out, Timeline* t) {
  memcpy(out, in, (size_t)n * sizeof(Job));
  for (int i = 0; i < n; i++) {
    out[i].status = JOB_NEW;
    out[i].remaining = out[i].burstTime;
  }
  switch (p) {
    case POLICY_FCFS:     return fcfs(out, n, t);
    case POLICY_RR:       return roundRobin(out, n, quantum, t);
    case POLICY_PRIORITY: return priorityScheduling(out, n, t);
  }
  return -1;
}

Metrics computeMetrics(const Job* jobs, int n) {
  Metrics m = {0, 0, 0};
  for (int i = 0; i < n; i++) {
    m.avgCT += jobs[i].completion;
    m.avgTAT += jobs[i].tat;
    m.avgWT += jobs[i].wt;
  }
  if (n > 0) {
    m.avgCT /= n;
    m.avgTAT /= n;
    m.avgWT /= n;
  }
  return m;
}

void printTimeline(const Timeline* t) {
  printf("Execution timeline:\n");
  for (int i = 0; i < t->count; i++) {
    if (t->slices[i].jobID == IDLE_ID)
      printf("  [%3d - %3d]  IDLE\n", t->slices[i].start, t->slices[i].end);
    else
      printf("  [%3d - %3d]  J%d\n", t->slices[i].start, t->slices[i].end, t->slices[i].jobID);
  }
  printf("Order: ");
  for (int i = 0; i < t->count; i++) {
    if (t->slices[i].jobID == IDLE_ID) printf("IDLE");
    else printf("J%d", t->slices[i].jobID);
    printf(i + 1 < t->count ? " -> " : "\n");
  }
}

void printMetricsTable(const Job* jobs, int n, const Metrics* m) {
  Job sorted[MAX_JOBS];
  memcpy(sorted, jobs, (size_t)n * sizeof(Job));
  qsort(sorted, n, sizeof(Job), byId);               /* show in job-ID order */

  printf("%-5s %-8s %-6s %-8s %-11s %-11s %-8s\n",
         "Job", "Arrival", "Burst", "Priority", "Completion", "Turnaround", "Waiting");
  for (int i = 0; i < n; i++) {
    printf("J%-4d %-8d %-6d %-8d %-11d %-11d %-8d\n", sorted[i].jobID, sorted[i].arrivalTime,
           sorted[i].burstTime, sorted[i].priority, sorted[i].completion, sorted[i].tat, sorted[i].wt);
  }
  printf("Average: completion=%.2f  turnaround=%.2f  waiting=%.2f\n", m->avgCT, m->avgTAT, m->avgWT);
}

int reportPolicy(Policy p, const Job* jobs, int n, int quantum) {
  Job work[MAX_JOBS];
  Timeline t;
  timelineInit(&t);

  char title[96];
  if (p == POLICY_RR) snprintf(title, sizeof(title), "%s  (time quantum = %d)", policyName(p), quantum);
  else snprintf(title, sizeof(title), "%s", policyName(p));
  printf("\n=== %s ===\n", title);
  printf("Tie-breaking rule: %s\n", policyTieRule(p));

  if (simulate(p, jobs, n, quantum, work, &t) != 0) {
    fprintf(stderr, "error: scheduling failed (invalid input or out of memory)\n");
    timelineFree(&t);
    return -1;
  }
  printTimeline(&t);
  Metrics m = computeMetrics(work, n);
  printMetricsTable(work, n, &m);
  timelineFree(&t);
  return 0;
}

int comparePolicies(const Job* jobs, int n, int quantum) {
  static const Policy all[] = {POLICY_FCFS, POLICY_RR, POLICY_PRIORITY};
  Metrics res[3];
  Job work[MAX_JOBS];

  for (int k = 0; k < 3; k++) {
    Timeline t;
    timelineInit(&t);
    if (simulate(all[k], jobs, n, quantum, work, &t) != 0) {
      fprintf(stderr, "error: %s failed\n", policyName(all[k]));
      timelineFree(&t);
      return -1;
    }
    res[k] = computeMetrics(work, n);
    timelineFree(&t);
  }

  printf("\n=== Scheduling comparison (same %d jobs, RR quantum = %d) ===\n", n, quantum);
  printf("%-28s %-8s %-11s %-8s\n", "Policy", "Avg CT", "Avg TAT", "Avg WT");
  int bestWT = 0, bestTAT = 0;
  for (int k = 0; k < 3; k++) {
    printf("%-28s %-8.2f %-11.2f %-8.2f\n", policyName(all[k]), res[k].avgCT, res[k].avgTAT, res[k].avgWT);
    if (res[k].avgWT < res[bestWT].avgWT) bestWT = k;
    if (res[k].avgTAT < res[bestTAT].avgTAT) bestTAT = k;
  }
  printf("Lowest average waiting time:    %s\n", policyName(all[bestWT]));
  printf("Lowest average turnaround time: %s\n", policyName(all[bestTAT]));
  return 0;
}
