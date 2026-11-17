#include "../include/job.h"
#include "../include/campus.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static Job j[MAX_JOBS];
static int jobCount = 0;

static Slice timeline[MAX_SLICES];
static int sliceCount = 0;
static int sliceOverflow = 0;

static double avgCT = 0, avgTAT = 0, avgWT = 0;
static int quiet = 0;                 // 1 = run an algorithm without printing (used by compare)

// ---------- ready queue (linked list of job indexes, used by Round Robin) ----------

static int dequeue(Node** head, Node** tail) {
  if ((*head) == NULL) return -1;

  Node* temp = *head;

  *head = (*head)->next;
  if ((*head) == NULL) (*tail) = NULL;
  int index = temp->index;
  free(temp);
  return index;
}

static void enqueue(int index, Node** head, Node** tail) {
  if (index < 0 || index >= jobCount) {
    fprintf(stderr, "internal error: incorrect queue index\n");
    return;
  }

  Node* new = malloc(sizeof(Node));
  if (new == NULL) {
    fprintf(stderr, "error: malloc failed\n");
    return;
  }

  new->index = index;
  new->next = NULL;
  if ((*head) == NULL) {
    *head = new;
    *tail = new;
    return;
  }

  (*tail)->next = new;
  (*tail) = (*tail)->next;
}

// ---------- job management (FR1) ----------

int addJob(int id, int arrival, int burst, int priority) {
  if (jobCount >= MAX_JOBS) {
    fprintf(stderr, "error: cannot add job (limit %d)\n", MAX_JOBS);
    return 1;
  }
  if (id <= 0 || id > MAX_TIME_VALUE) {
    fprintf(stderr, "error: job ID must be 1..%d (got %d)\n", MAX_TIME_VALUE, id);
    return 1;
  }
  if (arrival < 0 || arrival > MAX_TIME_VALUE) {
    fprintf(stderr, "error: arrival time must be 0..%d (got %d)\n", MAX_TIME_VALUE, arrival);
    return 1;
  }
  if (burst <= 0 || burst > MAX_TIME_VALUE) {
    fprintf(stderr, "error: CPU burst must be 1..%d (got %d)\n", MAX_TIME_VALUE, burst);
    return 1;
  }
  if (priority < 0 || priority > MAX_TIME_VALUE) {
    fprintf(stderr, "error: priority must be 0..%d (got %d)\n", MAX_TIME_VALUE, priority);
    return 1;
  }

  for (int i = 0; i < jobCount; i++) {
    if (j[i].id == id) {
      fprintf(stderr, "error: job ID %d already exists\n", id);
      return 1;
    }
  }

  Job new;
  new.id = id;
  new.arrival = arrival;
  new.burst = burst;
  new.priority = priority;
  new.remaining = burst;
  new.completion = 0;
  new.status = READY;
  new.waitTime = -1;
  new.totTime = -1;

  j[jobCount] = new;
  jobCount++;
  return 0;
}

static const char* statusName(int s) {
  if (s == READY) return "READY";
  if (s == RUNNING) return "RUNNING";
  if (s == COMPLETED) return "COMPLETED";
  return "?";
}

static void printCell(int v) {
  if (v < 0) printf("%-10s ", "-");
  else printf("%-10d ", v);
}

void displayJob(void) {
  if (jobCount == 0) {
    printf("No jobs to display\n");
    return;
  }

  printf("%-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n",
      "JOB", "Arrival", "Burst", "Priority", "Status", "CT", "TAT", "WT");

  for (int i = 0; i < jobCount; i++) {
    printf("%-10d %-10d %-10d %-10d %-10s ", j[i].id, j[i].arrival, j[i].burst,
        j[i].priority, statusName(j[i].status));
    printCell(j[i].status == COMPLETED ? j[i].completion : -1);
    printCell(j[i].totTime);
    printCell(j[i].waitTime);
    printf("\n");
  }
}

// reset the run state of every job and clear the timeline (no printing)
static void resetState(void) {
  for (int i = 0; i < jobCount; i++) {
    j[i].remaining = j[i].burst;
    j[i].status = READY;
    j[i].completion = 0;
    j[i].totTime = -1;
    j[i].waitTime = -1;
  }
  sliceCount = 0;
  sliceOverflow = 0;
}

void resetJob(void) {
  if (jobCount == 0) {
    printf("No jobs to reset\n");
    return;
  }
  resetState();
  printf("Reset successful\n");
}

int loadDefaultJobs(void) {
  static const int data[][4] = {
    {1, 0, 5, 3}, {2, 1, 3, 1}, {3, 2, 8, 4}, {4, 3, 6, 2}, {5, 4, 2, 1},
  };
  jobCount = 0;
  for (unsigned i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    if (addJob(data[i][0], data[i][1], data[i][2], data[i][3]) != 0) return 1;
  }
  return 0;
}

static int onlySpace(const char* s) {
  for (; *s; s++)
    if (!isspace((unsigned char)*s)) return 0;
  return 1;
}

// file format: one job per line "jobID arrival burst priority", '#' starts a comment
int loadJobsFile(const char* path) {
  FILE* fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
    return -1;
  }

  Job backup[MAX_JOBS];                 // restore the old list if the file is bad
  int backupCount = jobCount;
  memcpy(backup, j, sizeof(j));
  jobCount = 0;

  char line[256];
  int lineNo = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    lineNo++;
    char* hash = strchr(line, '#');
    if (hash != NULL) *hash = '\0';
    if (onlySpace(line)) continue;

    long id, arr, burst, prio;
    int used = 0;
    int got = sscanf(line, "%ld %ld %ld %ld %n", &id, &arr, &burst, &prio, &used);
    if (got != 4 || !onlySpace(line + used)) {
      fprintf(stderr, "error: %s line %d: expected 'jobID arrival burst priority' (4 integers)\n", path, lineNo);
      goto fail;
    }
    if (id < 0 || id > MAX_TIME_VALUE || arr < -1 || arr > MAX_TIME_VALUE ||
        burst < -1 || burst > MAX_TIME_VALUE || prio < -1 || prio > MAX_TIME_VALUE) {
      fprintf(stderr, "error: %s line %d: value out of range\n", path, lineNo);
      goto fail;
    }
    if (addJob((int)id, (int)arr, (int)burst, (int)prio) != 0) {
      fprintf(stderr, "       (in %s line %d)\n", path, lineNo);
      goto fail;
    }
  }
  fclose(fp);

  if (jobCount == 0) {
    fprintf(stderr, "error: '%s' contains no jobs\n", path);
    jobCount = backupCount;
    memcpy(j, backup, sizeof(j));
    return -1;
  }
  return jobCount;

fail:
  fclose(fp);
  jobCount = backupCount;
  memcpy(j, backup, sizeof(j));
  return -1;
}

// for the IPC demo: the first job that finished in the last scheduling run
int firstCompletedJob(int* id, int* arrival, int* completion) {
  for (int i = 0; i < jobCount; i++) {
    if (j[i].status == COMPLETED) {
      *id = j[i].id;
      *arrival = j[i].arrival;
      *completion = j[i].completion;
      return 0;
    }
  }
  return -1;
}

// ---------- scheduling helpers ----------

// true if job a should run before job b: earlier arrival first, tie -> lower ID
static int before(int a, int b) {
  if (j[a].arrival != j[b].arrival) return j[a].arrival < j[b].arrival;
  return j[a].id < j[b].id;
}

static void sortByArrival(int order[], int n) {
  for (int i = 1; i < n; i++) {
    int key = order[i];
    int k = i - 1;
    while (k >= 0 && before(key, order[k])) {
      order[k + 1] = order[k];
      k--;
    }
    order[k + 1] = key;
  }
}

static void addSlice(int id, int start, int end) {
  if (sliceCount >= MAX_SLICES) {
    sliceOverflow = 1;
    return;
  }
  timeline[sliceCount].id = id;
  timeline[sliceCount].start = start;
  timeline[sliceCount].end = end;
  sliceCount++;
}

// run job `index` for `duration` time units starting at *clk
static void execute(int* clk, int duration, int index) {
  if (index < 0 || index >= jobCount) {
    fprintf(stderr, "internal error: wrong index\n");
    return;
  }
  if (duration <= 0 || duration > j[index].remaining) {
    fprintf(stderr, "internal error: incorrect duration\n");
    return;
  }
  if (j[index].arrival > *clk) {
    fprintf(stderr, "internal error: job not arrived\n");
    return;
  }
  if (j[index].status != READY) return;

  j[index].status = RUNNING;
  addSlice(j[index].id, *clk, *clk + duration);
  *clk += duration;
  j[index].remaining -= duration;

  if (j[index].remaining == 0) {
    j[index].status = COMPLETED;
    j[index].completion = *clk;
    return;
  }

  j[index].status = READY;
}

// TAT = completion - arrival, WT = TAT - burst, plus the averages
static void calculateTime(void) {
  double ctSum = 0, totSum = 0, waitSum = 0;
  for (int i = 0; i < jobCount; i++) {
    j[i].totTime = j[i].completion - j[i].arrival;
    j[i].waitTime = j[i].totTime - j[i].burst;
    ctSum += j[i].completion;
    totSum += j[i].totTime;
    waitSum += j[i].waitTime;
  }
  avgCT = ctSum / jobCount;
  avgTAT = totSum / jobCount;
  avgWT = waitSum / jobCount;
}

static void printReport(const char* name, int quantum, const char* tieRule) {
  if (quiet) return;

  if (quantum > 0) printf("\n=== %s (time quantum = %d) ===\n", name, quantum);
  else printf("\n=== %s ===\n", name);
  printf("Tie-breaking rule: %s\n", tieRule);

  printf("Execution timeline:\n");
  for (int i = 0; i < sliceCount; i++) {
    if (timeline[i].id == IDLE_ID)
      printf("  [%3d - %3d]  IDLE\n", timeline[i].start, timeline[i].end);
    else
      printf("  [%3d - %3d]  J%d\n", timeline[i].start, timeline[i].end, timeline[i].id);
  }
  printf("Order: ");
  for (int i = 0; i < sliceCount; i++) {
    if (timeline[i].id == IDLE_ID) printf("IDLE");
    else printf("J%d", timeline[i].id);
    printf(i + 1 < sliceCount ? " -> " : "\n");
  }
  if (sliceOverflow) printf("(timeline truncated after %d entries)\n", MAX_SLICES);

  displayJob();
  printf("Average: completion=%.2f  turnaround=%.2f  waiting=%.2f\n", avgCT, avgTAT, avgWT);
}

static int haveJobs(void) {
  if (jobCount == 0) {
    fprintf(stderr, "error: no jobs loaded (add jobs or load the default set first)\n");
    return 0;
  }
  return 1;
}

// ---------- the three algorithms (FR2) ----------

int fcfs(void) {
  if (!haveJobs()) return 1;
  resetState();

  int order[MAX_JOBS];
  for (int i = 0; i < jobCount; i++) order[i] = i;
  sortByArrival(order, jobCount);

  int clk = 0;
  for (int i = 0; i < jobCount; i++) {
    int index = order[i];

    if (clk < j[index].arrival) {              // CPU idle until the job arrives
      addSlice(IDLE_ID, clk, j[index].arrival);
      clk = j[index].arrival;
    }
    execute(&clk, j[index].remaining, index);  // non-preemptive: runs to completion
  }

  calculateTime();
  printReport("FCFS", 0, "equal arrival time -> lower job ID first");
  return 0;
}

int roundRobin(int quantum) {
  if (quantum <= 0) {
    fprintf(stderr, "error: time quantum must be a positive integer (got %d)\n", quantum);
    return 1;
  }
  if (!haveJobs()) return 1;
  resetState();

  int clk = 0;
  int completed = 0;
  int nextArrival = 0;                         // first job (in arrival order) not yet queued

  Node* rrHead = NULL;
  Node* rrTail = NULL;

  int order[MAX_JOBS];
  for (int i = 0; i < jobCount; i++) order[i] = i;
  sortByArrival(order, jobCount);

  while (completed < jobCount) {
    while (nextArrival < jobCount && j[order[nextArrival]].arrival <= clk)
      enqueue(order[nextArrival++], &rrHead, &rrTail);

    if (rrHead == NULL) {                      // nobody ready: idle until the next arrival
      if (nextArrival < jobCount) {
        addSlice(IDLE_ID, clk, j[order[nextArrival]].arrival);
        clk = j[order[nextArrival]].arrival;
      }
      continue;
    }

    int index = dequeue(&rrHead, &rrTail);
    int duration = quantum;
    if (j[index].remaining < quantum) duration = j[index].remaining;

    execute(&clk, duration, index);

    // jobs that arrived during this slice are queued BEFORE the preempted job
    while (nextArrival < jobCount && j[order[nextArrival]].arrival <= clk)
      enqueue(order[nextArrival++], &rrHead, &rrTail);

    if (j[index].remaining == 0) completed++;
    else enqueue(index, &rrHead, &rrTail);
  }

  calculateTime();
  printReport("Round Robin", quantum,
      "equal arrival -> lower job ID; jobs arriving during a slice queue before the preempted job");
  return 0;
}

// non-preemptive: lower priority number = higher priority
int prioritySched(void) {
  if (!haveJobs()) return 1;
  resetState();

  int clk = 0;
  int completed = 0;

  while (completed < jobCount) {
    int selected = -1;

    for (int i = 0; i < jobCount; i++) {
      if (j[i].arrival > clk || j[i].status == COMPLETED) continue;

      if (selected == -1) {
        selected = i;
        continue;
      }

      if (j[i].priority < j[selected].priority)
        selected = i;
      else if (j[i].priority == j[selected].priority) {
        if (j[i].arrival < j[selected].arrival ||
            (j[i].arrival == j[selected].arrival && j[i].id < j[selected].id))
          selected = i;
      }
    }

    if (selected == -1) {                      // nothing ready: idle until the next arrival
      int nextArrival = -1;
      for (int i = 0; i < jobCount; i++) {
        if (j[i].status == COMPLETED) continue;
        if (nextArrival == -1 || j[i].arrival < nextArrival) nextArrival = j[i].arrival;
      }
      if (nextArrival == -1) break;
      addSlice(IDLE_ID, clk, nextArrival);
      clk = nextArrival;
      continue;
    }

    execute(&clk, j[selected].remaining, selected);   // runs to completion
    if (j[selected].status == COMPLETED) completed++;
  }

  calculateTime();
  printReport("Priority (non-preemptive)", 0,
      "lower number = higher priority; ties -> earlier arrival, then lower job ID");
  return 0;
}

// same job set under all three policies, side by side (S09)
int compareSchedulers(int quantum) {
  if (quantum <= 0) {
    fprintf(stderr, "error: time quantum must be a positive integer (got %d)\n", quantum);
    return 1;
  }
  if (!haveJobs()) return 1;

  double ct[3], tat[3], wt[3];
  const char* names[3] = {"FCFS", "Round Robin", "Priority (non-preemptive)"};

  quiet = 1;
  int rc = fcfs();
  ct[0] = avgCT; tat[0] = avgTAT; wt[0] = avgWT;
  rc |= roundRobin(quantum);
  ct[1] = avgCT; tat[1] = avgTAT; wt[1] = avgWT;
  rc |= prioritySched();
  ct[2] = avgCT; tat[2] = avgTAT; wt[2] = avgWT;
  quiet = 0;
  if (rc) return 1;

  printf("\n=== Scheduling comparison (same %d jobs, RR quantum = %d) ===\n", jobCount, quantum);
  printf("%-28s %-9s %-11s %-9s\n", "Policy", "Avg CT", "Avg TAT", "Avg WT");
  int bestWT = 0, bestTAT = 0;
  for (int k = 0; k < 3; k++) {
    printf("%-28s %-9.2f %-11.2f %-9.2f\n", names[k], ct[k], tat[k], wt[k]);
    if (wt[k] < wt[bestWT]) bestWT = k;
    if (tat[k] < tat[bestTAT]) bestTAT = k;
  }
  printf("Lowest average waiting time:    %s\n", names[bestWT]);
  printf("Lowest average turnaround time: %s\n", names[bestTAT]);
  return 0;
}

int main(void) {
  loadDefaultJobs();
  fcfs();
  roundRobin(2);
  prioritySched();
  compareSchedulers(2);
  return 0;
}
