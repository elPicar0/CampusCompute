#include "../include/demo.h"

#include <stdio.h>

#include "../include/banker.h"
#include "../include/ipc.h"
#include "../include/jobio.h"
#include "../include/limits.h"
#include "../include/logger.h"
#include "../include/scheduler.h"
#include "../include/util.h"
#include "../include/verify.h"
#include "../include/worker.h"

#define MAIN_LOG "logs/campus.log"
#define MAX_STAGES 12

typedef struct {
  const char* name;
  int passed;
} StageResult;

static StageResult stages[MAX_STAGES];
static int stageCount;

static void record(const char* name, int ok, Logger* lg) {
  stages[stageCount++] = (StageResult){name, ok};
  loggerLog(lg, "DEMO", "stage '%s': %s", name, ok ? "PASS" : "FAIL");
}

int runIntegratedDemo(void) {
  stageCount = 0;
  if (ensureDir("logs") != 0) return -1;

  printBanner("CampusCompute integrated demonstration");
  Logger* lg = loggerOpen(MAIN_LOG, 8, 0, 0);
  if (lg == NULL) return -1;
  loggerLog(lg, "DEMO", "integrated demonstration started");

  /* 1. job management + scheduling */
  Job jobs[MAX_JOBS];
  int n = defaultJobs(jobs, MAX_JOBS);
  printf("\nStage 1: job management and CPU scheduling\n");
  printJobTable(jobs, n);
  int ok = 1;
  for (int p = POLICY_FCFS; p <= POLICY_PRIORITY; p++) {
    ok &= (reportPolicy((Policy)p, jobs, n, DEFAULT_QUANTUM) == 0);
    loggerLog(lg, "SCHED", "%s simulated for %d jobs", policyName((Policy)p), n);
  }
  ok &= (comparePolicies(jobs, n, DEFAULT_QUANTUM) == 0);
  record("job scheduling (FCFS, RR, Priority, comparison)", ok, lg);

  /* 2. concurrent logger with several producer threads */
  printBanner("Stage 2: concurrent producer-consumer logger");
  ok = (runLoggerDemo(3, 4, 3, "logs/logger_demo.log") == 0);
  record("concurrent logger (pthreads, mutex, condition variables)", ok, lg);

  /* 3. shared memory + named semaphores, carrying real FCFS results */
  printBanner("Stage 3: shared-memory IPC with named semaphores");
  Job out[MAX_JOBS];
  Timeline t;
  timelineInit(&t);
  int values[MAX_JOBS];
  ok = (simulate(POLICY_FCFS, jobs, n, DEFAULT_QUANTUM, out, &t) == 0);
  timelineFree(&t);
  for (int i = 0; ok && i < n; i++) values[i] = out[i].tat;
  ok = ok && (runIpcDemo(values, n, lg) == 0);
  record("shared-memory IPC and semaphore cleanup", ok, lg);

  /* 4. Banker's algorithm: one safe and one unsafe state */
  printBanner("Stage 4: Banker's algorithm");
  BankerState s;
  bankerSampleSafe(&s);
  int safe = bankerAnalyze(&s, "sample SAFE state", lg);
  bankerSampleUnsafe(&s);
  int unsafe = bankerAnalyze(&s, "sample UNSAFE state", lg);
  record("Banker's: safe state accepted, unsafe state detected", safe == 1 && unsafe == 0, lg);

  /* 5. worker processes */
  printBanner("Stage 5: worker processes (fork/waitpid)");
  ok = (runWorkerPool(lg) == 0);
  record("worker pool: normal exits and simulated failures detected", ok, lg);

  /* 6. resource limits */
  printBanner("Stage 6: resource limits (setrlimit)");
  ok = (runLimitDemo(LIMIT_NOFILE, lg) == 0);
  ok &= (runLimitDemo(LIMIT_MEM, lg) == 0);
  ok &= (runLimitDemo(LIMIT_CPU, lg) == 0);
  record("resource limits: RLIMIT_NOFILE, RLIMIT_AS, RLIMIT_CPU", ok, lg);

  loggerLog(lg, "DEMO", "all stages executed, closing logger");
  int logOk = (loggerClose(lg) == 0);          /* joins the consumer thread */
  int lines = countLines(MAIN_LOG);
  printf("\nMain log '%s' written: %d entries\n", MAIN_LOG, lines);
  record("logger flushed and consumer thread joined", logOk && lines > 0, NULL);

  /* 7. cleanup verification, after the logger thread is gone */
  ok = (verifyCleanup() == 0);
  record("cleanup: no threads, children or IPC objects left", ok, NULL);

  printBanner("INTEGRATED SUMMARY");
  int failed = 0;
  for (int i = 0; i < stageCount; i++) {
    printf("[%s] %s\n", stages[i].passed ? "PASS" : "FAIL", stages[i].name);
    if (!stages[i].passed) failed++;
  }
  printf("\n%d of %d stages passed. Log file: %s\n", stageCount - failed, stageCount, MAIN_LOG);
  return failed == 0 ? 0 : -1;
}
