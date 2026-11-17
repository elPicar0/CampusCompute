#ifndef WORKER_H
#define WORKER_H

#include <sys/types.h>

#include "logger.h"

/* FR6. Worker processes are created with fork(); the parent collects each
 * status with waitpid(). Failures are SIMULATED on purpose (the child calls
 * _exit(3) or raises SIGTERM itself) and are always labelled as such. */
typedef enum {
  WORKER_OK,           /* does its work, exits 0 */
  WORKER_FAIL_EXIT,    /* simulated failure: exits with code 3 */
  WORKER_FAIL_SIGNAL   /* simulated crash: kills itself with SIGTERM */
} WorkerMode;

#define SIMULATED_EXIT_CODE 3

typedef struct {
  int workerId;
  pid_t pid;
  WorkerMode mode;
  int exited, exitCode;      /* WIFEXITED / WEXITSTATUS */
  int signaled, signal;      /* WIFSIGNALED / WTERMSIG */
} WorkerResult;

/* Parent-side classification of a waitpid status. */
const char* workerVerdict(const WorkerResult* r);

/* Runs one worker to completion and fills *out. Returns 0 if the parent's
 * detected status matches what the worker was told to do. */
int runWorker(int workerId, WorkerMode mode, WorkerResult* out, Logger* lg);

/* Four workers running concurrently (2 normal, 1 simulated exit failure,
 * 1 simulated signal crash), reaped with waitpid(-1, ...). Returns 0 if all
 * results were detected correctly. */
int runWorkerPool(Logger* lg);

#endif
