#include <stdio.h>

#include "../include/campus.h"

#define MAX_STAGES 12
#define DEMO_QUANTUM 2

static const char* stageName[MAX_STAGES];
static int stagePassed[MAX_STAGES];
static int stageCount = 0;

static void record(const char* name, int rc) {
  stageName[stageCount] = name;
  stagePassed[stageCount] = (rc == 0);
  stageCount++;
}

// runs every feature in order and prints a PASS/FAIL summary (S21)
int runDemo(void) {
  stageCount = 0;
  printf("\n=== CampusCompute integrated demonstration ===\n");

  printf("\n>>> Stage 1: job management and CPU scheduling\n");
  int rc = loadDefaultJobs();
  displayJob();
  rc |= fcfs();
  rc |= roundRobin(DEMO_QUANTUM);
  rc |= prioritySched();
  rc |= compareSchedulers(DEMO_QUANTUM);
  record("job management and scheduling (FCFS, Round Robin, Priority, comparison)", rc);

  printf("\n>>> Stage 2: concurrent activity logger\n");
  record("producer-consumer logger (pthreads, mutex, condition variables)", runLogger());

  printf("\n>>> Stage 3: shared-memory IPC with a named semaphore\n");
  record("shared-memory IPC and semaphore cleanup", runIPC());

  printf("\n>>> Stage 4: Banker's algorithm\n");
  rc = banker_algo_success();
  rc |= banker_algo_deadlock();
  record("Banker's: safe state accepted, unsafe state detected", rc);

  printf("\n>>> Stage 5: worker processes\n");
  rc = success();
  rc |= failure();
  record("workers: normal exit and simulated failure detected", rc);

  printf("\n>>> Stage 6: resource limits and cleanup\n");
  record("resource limits (RLIMIT_CPU, RLIMIT_AS) and file cleanup", run_resource_demo());

  printf("\n>>> Stage 7: cleanup verification\n");
  record("no threads, children, files or IPC objects left behind", verify_cleanup());

  printf("\n=== INTEGRATED SUMMARY ===\n");
  int failed = 0;
  for (int i = 0; i < stageCount; i++) {
    printf("[%s] %s\n", stagePassed[i] ? "PASS" : "FAIL", stageName[i]);
    if (!stagePassed[i]) failed++;
  }
  printf("\n%d of %d stages passed. Log file: campus.log\n", stageCount - failed, stageCount);
  return failed == 0 ? 0 : 1;
}
