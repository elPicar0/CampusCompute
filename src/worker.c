#include "../include/worker.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char* modeName(WorkerMode m) {
  switch (m) {
    case WORKER_OK:          return "normal";
    case WORKER_FAIL_EXIT:   return "SIMULATED failure (exit code 3)";
    case WORKER_FAIL_SIGNAL: return "SIMULATED crash (SIGTERM)";
  }
  return "?";
}

/* Child body. Never returns. */
static void workerChild(int id, WorkerMode mode) {
  printf("  [worker %d pid=%d] started, doing CPU work\n", id, (int)getpid());
  volatile long acc = 0;
  for (long i = 1; i <= 2000000L; i++) acc += i % 7;   /* small real computation */
  usleep(20000 * (unsigned)id);                         /* stagger finishing times */

  if (mode == WORKER_FAIL_EXIT) {
    printf("  [worker %d pid=%d] *** SIMULATED FAILURE (deliberate, not a real OS fault): exiting with code %d\n",
           id, (int)getpid(), SIMULATED_EXIT_CODE);
    fflush(stdout);
    _exit(SIMULATED_EXIT_CODE);
  }
  if (mode == WORKER_FAIL_SIGNAL) {
    printf("  [worker %d pid=%d] *** SIMULATED CRASH (deliberate, not a real OS fault): raising SIGTERM on itself\n",
           id, (int)getpid());
    fflush(stdout);
    raise(SIGTERM);
    _exit(99);                                          /* not reached */
  }
  printf("  [worker %d pid=%d] finished normally (checksum %ld)\n", id, (int)getpid(), (long)acc);
  fflush(stdout);
  _exit(0);
}

static void decode(WorkerResult* r, int status) {
  r->exited = WIFEXITED(status);
  r->exitCode = r->exited ? WEXITSTATUS(status) : -1;
  r->signaled = WIFSIGNALED(status);
  r->signal = r->signaled ? WTERMSIG(status) : 0;
}

const char* workerVerdict(const WorkerResult* r) {
  if (r->exited && r->exitCode == 0) return "COMPLETED NORMALLY";
  if (r->exited) return "FAILED (non-zero exit status)";
  if (r->signaled) return "FAILED (terminated by signal)";
  return "UNKNOWN";
}

static int matchesExpectation(const WorkerResult* r) {
  switch (r->mode) {
    case WORKER_OK:          return r->exited && r->exitCode == 0;
    case WORKER_FAIL_EXIT:   return r->exited && r->exitCode == SIMULATED_EXIT_CODE;
    case WORKER_FAIL_SIGNAL: return r->signaled && r->signal == SIGTERM;
  }
  return 0;
}

static void reportResult(const WorkerResult* r, Logger* lg) {
  printf("[parent] worker %d (pid %d, mode: %s): %s", r->workerId, (int)r->pid, modeName(r->mode), workerVerdict(r));
  if (r->exited) printf(", exit code %d", r->exitCode);
  if (r->signaled) printf(", signal %d (%s)", r->signal, strsignal(r->signal));
  printf("%s\n", r->mode == WORKER_OK ? "" : "  [failure was SIMULATED]");
  loggerLog(lg, "WORKER", "worker %d pid %d: %s%s", r->workerId, (int)r->pid, workerVerdict(r),
            r->mode == WORKER_OK ? "" : " (simulated)");
}

int runWorker(int workerId, WorkerMode mode, WorkerResult* out, Logger* lg) {
  WorkerResult r = {.workerId = workerId, .mode = mode};
  fflush(stdout);
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }
  if (pid == 0) workerChild(workerId, mode);

  r.pid = pid;
  printf("[parent pid=%d] forked worker %d as pid %d, waiting with waitpid()\n", (int)getpid(), workerId, (int)pid);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("waitpid");
      return -1;
    }
  }
  decode(&r, status);
  reportResult(&r, lg);
  if (out != NULL) *out = r;
  return matchesExpectation(&r) ? 0 : -1;
}

int runWorkerPool(Logger* lg) {
  enum { N = 4 };
  static const WorkerMode modes[N] = {WORKER_OK, WORKER_FAIL_EXIT, WORKER_OK, WORKER_FAIL_SIGNAL};
  WorkerResult res[N];
  memset(res, 0, sizeof(res));
  int spawned = 0;

  fflush(stdout);
  for (int i = 0; i < N; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      break;
    }
    if (pid == 0) workerChild(i + 1, modes[i]);
    res[i] = (WorkerResult){.workerId = i + 1, .pid = pid, .mode = modes[i]};
    printf("[parent pid=%d] forked worker %d as pid %d\n", (int)getpid(), i + 1, (int)pid);
    spawned++;
  }

  int ok = 0, failed = 0, bad = 0;
  for (int k = 0; k < spawned; k++) {
    int status = 0;
    pid_t done = waitpid(-1, &status, 0);              /* whichever worker ends first */
    if (done < 0) {
      if (errno == EINTR) { k--; continue; }
      perror("waitpid");
      bad++;
      break;
    }
    for (int i = 0; i < spawned; i++) {
      if (res[i].pid != done) continue;
      decode(&res[i], status);
      reportResult(&res[i], lg);
      if (!matchesExpectation(&res[i])) bad++;
      else if (res[i].mode == WORKER_OK) ok++;
      else failed++;
    }
  }
  printf("[parent] pool summary: %d completed normally, %d failed (all simulated), %d unexpected\n", ok, failed, bad);
  return (spawned == N && bad == 0) ? 0 : -1;
}
