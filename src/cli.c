#include "../include/cli.h"

#include <stdio.h>
#include <string.h>

#include "../include/banker.h"
#include "../include/demo.h"
#include "../include/ipc.h"
#include "../include/job.h"
#include "../include/jobio.h"
#include "../include/limits.h"
#include "../include/logger.h"
#include "../include/scheduler.h"
#include "../include/util.h"
#include "../include/verify.h"
#include "../include/worker.h"

void cliPrintUsage(void) {
  printf(
    "CampusCompute - job scheduling and resource management prototype\n"
    "\n"
    "Usage: campus <command> [arguments]\n"
    "\n"
    "Scheduling (FR1, FR2)\n"
    "  sched fcfs     [-f jobs.txt]               First Come First Served\n"
    "  sched rr       [-f jobs.txt] [-q quantum]  Round Robin (default quantum %d)\n"
    "  sched priority [-f jobs.txt]               Non-preemptive priority (lower number = higher priority)\n"
    "  sched compare  [-f jobs.txt] [-q quantum]  All three policies on the same job set\n"
    "      Without -f a built-in 5-job set is used. File format: 'jobID arrival burst priority'.\n"
    "\n"
    "Concurrency and IPC (FR3, FR4)\n"
    "  logger [producers items capacity]          Producer-consumer logger (defaults 3 4 3)\n"
    "  ipc                                        Shared memory + named semaphores between 2 processes\n"
    "\n"
    "Resource safety (FR5)\n"
    "  banker [safe|unsafe|<file>]                Banker's safety algorithm (default: both samples)\n"
    "\n"
    "Processes and limits (FR6, FR7)\n"
    "  worker [ok|fail|crash|pool]                fork()/waitpid() workers (failures are SIMULATED)\n"
    "  limits [nofile|mem|cpu|all]                setrlimit() demonstrations\n"
    "  verify                                     Check no threads/children/IPC objects are left behind\n"
    "  cleanup                                    Remove stale IPC objects named %s* only\n"
    "\n"
    "Everything\n"
    "  demo                                       Integrated run of all features with a summary\n"
    "  help                                       This message\n"
    "\n"
    "Exit codes: 0 success, 1 runtime/input error, 2 usage error.\n",
    DEFAULT_QUANTUM, IPC_NAME_PREFIX);
}

static int usageError(const char* fmt, const char* arg) {
  fprintf(stderr, "error: ");
  fprintf(stderr, fmt, arg);
  fprintf(stderr, "\nRun './campus help' for usage.\n");
  return EXIT_USAGE;
}

// sched

static int cmdSched(int argc, char** argv) {
  if (argc < 3)
    return usageError("%s", "sched needs a policy: fcfs | rr | priority | compare");

  const char* policy = argv[2];
  const char* file = NULL;
  int quantum = DEFAULT_QUANTUM;
  int quantumGiven = 0;

  for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
      file = argv[++i];
    } else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc) {
      if (parseInt(argv[++i], 1, MAX_TIME_VALUE, &quantum) != 0)
        return usageError("invalid quantum '%s' (must be an integer 1..100000)", argv[i]);
      quantumGiven = 1;
    } else {
      return usageError("unknown or incomplete option '%s'", argv[i]);
    }
  }

  int policyId;
  if (strcmp(policy, "fcfs") == 0) policyId = POLICY_FCFS;
  else if (strcmp(policy, "rr") == 0) policyId = POLICY_RR;
  else if (strcmp(policy, "priority") == 0) policyId = POLICY_PRIORITY;
  else if (strcmp(policy, "compare") == 0) policyId = -1;
  else return usageError("unknown scheduling policy '%s' (use fcfs, rr, priority, compare)", policy);

  if (quantumGiven && policyId != POLICY_RR && policyId != -1)
    return usageError("%s", "-q only applies to 'rr' and 'compare'");

  Job jobs[MAX_JOBS];
  int n = file ? loadJobs(file, jobs, MAX_JOBS) : defaultJobs(jobs, MAX_JOBS);
  if (n < 0) return EXIT_RUNTIME;

  printf("Input jobs%s:\n", file ? "" : " (built-in set)");
  printJobTable(jobs, n);

  if (policyId == -1) {
    int rc = 0;
    for (int p = POLICY_FCFS; p <= POLICY_PRIORITY; p++)
      rc |= reportPolicy((Policy)p, jobs, n, quantum);
    rc |= comparePolicies(jobs, n, quantum);
    return rc ? EXIT_RUNTIME : EXIT_OK;
  }
  return reportPolicy((Policy)policyId, jobs, n, quantum) == 0 ? EXIT_OK : EXIT_RUNTIME;
}

// logger / ipc 

static int cmdLogger(int argc, char** argv) {
  int producers = 3, items = 4, capacity = 3;
  if (argc != 2 && argc != 5)
    return usageError("%s", "logger takes either no arguments or: producers items capacity");
  if (argc == 5) {
    if (parseInt(argv[2], 1, 16, &producers) != 0)
      return usageError("producers must be an integer 1..16 (got '%s')", argv[2]);
    if (parseInt(argv[3], 1, 1000, &items) != 0)
      return usageError("items must be an integer 1..1000 (got '%s')", argv[3]);
    if (parseInt(argv[4], 1, 64, &capacity) != 0)
      return usageError("capacity must be an integer 1..64 (got '%s')", argv[4]);
  }
  return runLoggerDemo(producers, items, capacity, "logs/logger_demo.log") == 0 ? EXIT_OK : EXIT_RUNTIME;
}

static int cmdIpc(int argc, char** argv) {
  (void)argv;
  if (argc != 2) return usageError("%s", "ipc takes no arguments");

  Job jobs[MAX_JOBS], out[MAX_JOBS];
  Timeline t;
  int n = defaultJobs(jobs, MAX_JOBS);
  timelineInit(&t);
  if (simulate(POLICY_FCFS, jobs, n, DEFAULT_QUANTUM, out, &t) != 0) {
    timelineFree(&t);
    return EXIT_RUNTIME;
  }
  timelineFree(&t);
  int values[MAX_JOBS];
  for (int i = 0; i < n; i++) values[i] = out[i].tat;

  printf("Exchanging the FCFS turnaround times of %d jobs between two processes\n", n);
  return runIpcDemo(values, n, NULL) == 0 ? EXIT_OK : EXIT_RUNTIME;
}

// banker 

static int cmdBanker(int argc, char** argv) {
  if (argc > 3) return usageError("%s", "banker takes at most one argument");
  BankerState s;
  const char* what = argc == 3 ? argv[2] : "both";

  if (strcmp(what, "both") == 0 || strcmp(what, "safe") == 0) {
    bankerSampleSafe(&s);
    if (bankerAnalyze(&s, "sample SAFE state", NULL) < 0) return EXIT_RUNTIME;
  }
  if (strcmp(what, "both") == 0 || strcmp(what, "unsafe") == 0) {
    bankerSampleUnsafe(&s);
    if (bankerAnalyze(&s, "sample UNSAFE state", NULL) < 0) return EXIT_RUNTIME;
  }
  if (strcmp(what, "both") != 0 && strcmp(what, "safe") != 0 && strcmp(what, "unsafe") != 0) {
    if (bankerLoadFile(what, &s) != 0) return EXIT_RUNTIME;
    if (bankerAnalyze(&s, what, NULL) < 0) return EXIT_RUNTIME;
  }
  return EXIT_OK;
}

// worker 

static int cmdWorker(int argc, char** argv) {
  if (argc > 3) return usageError("%s", "worker takes at most one argument");
  const char* what = argc == 3 ? argv[2] : "pool";
  int rc;
  if (strcmp(what, "ok") == 0) rc = runWorker(1, WORKER_OK, NULL, NULL);
  else if (strcmp(what, "fail") == 0) rc = runWorker(1, WORKER_FAIL_EXIT, NULL, NULL);
  else if (strcmp(what, "crash") == 0) rc = runWorker(1, WORKER_FAIL_SIGNAL, NULL, NULL);
  else if (strcmp(what, "pool") == 0) rc = runWorkerPool(NULL);
  else return usageError("unknown worker mode '%s' (use ok, fail, crash, pool)", what);
  return rc == 0 ? EXIT_OK : EXIT_RUNTIME;
}

static int cmdLimits(int argc, char** argv) {
  if (argc > 3) return usageError("%s", "limits takes at most one argument");
  const char* what = argc == 3 ? argv[2] : "all";
  int rc = 0;
  if (strcmp(what, "nofile") == 0) rc = runLimitDemo(LIMIT_NOFILE, NULL);
  else if (strcmp(what, "mem") == 0) rc = runLimitDemo(LIMIT_MEM, NULL);
  else if (strcmp(what, "cpu") == 0) rc = runLimitDemo(LIMIT_CPU, NULL);
  else if (strcmp(what, "all") == 0) {
    rc |= runLimitDemo(LIMIT_NOFILE, NULL);
    rc |= runLimitDemo(LIMIT_MEM, NULL);
    rc |= runLimitDemo(LIMIT_CPU, NULL);
  } else return usageError("unknown limit '%s' (use nofile, mem, cpu, all)", what);
  return rc == 0 ? EXIT_OK : EXIT_RUNTIME;
}

//dispatcher

int cliRun(int argc, char** argv) {
  if (argc < 2) {
    cliPrintUsage();
    return EXIT_USAGE;
  }
  const char* cmd = argv[1];

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
    cliPrintUsage();
    return EXIT_OK;
  }
  if (strcmp(cmd, "sched") == 0) return cmdSched(argc, argv);
  if (strcmp(cmd, "logger") == 0) return cmdLogger(argc, argv);
  if (strcmp(cmd, "ipc") == 0) return cmdIpc(argc, argv);
  if (strcmp(cmd, "banker") == 0) return cmdBanker(argc, argv);
  if (strcmp(cmd, "worker") == 0) return cmdWorker(argc, argv);
  if (strcmp(cmd, "limits") == 0) return cmdLimits(argc, argv);

  if (strcmp(cmd, "verify") == 0) {
    if (argc != 2) return usageError("%s", "verify takes no arguments");
    return verifyCleanup() == 0 ? EXIT_OK : EXIT_RUNTIME;
  }
  if (strcmp(cmd, "cleanup") == 0) {
    if (argc != 2) return usageError("%s", "cleanup takes no arguments");
    int removed = ipcCleanupStale();
    printf("cleanup: removed %d stale object(s) named %s*\n", removed, IPC_NAME_PREFIX);
    return EXIT_OK;
  }
  if (strcmp(cmd, "demo") == 0) {
    if (argc != 2) return usageError("%s", "demo takes no arguments");
    return runIntegratedDemo() == 0 ? EXIT_OK : EXIT_RUNTIME;
  }

  return usageError("unknown command '%s'", cmd);
}
