#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/campus.h"

#define SIMULATED_EXIT_CODE 2

// Forks one worker and collects its status with waitpid().
// simulateFailure = 1: the worker deliberately exits with a non-zero code.
// That failure is SIMULATED, not a real OS fault, and is labelled that way.
// Returns 0 if the parent's detected status matches what the worker was told to do.
static int runWorker(int simulateFailure) {
  fflush(stdout);                       // avoid duplicating buffered output into the child
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    printf("  [worker pid=%d] started, doing work: ", (int)getpid());
    long sum = 0;
    for (int i = 1; i <= 5; i++) {
      sum += i;
      printf("%d ", i);
    }
    printf("(sum=%ld)\n", sum);

    if (simulateFailure) {
      printf("  [worker pid=%d] *** SIMULATED FAILURE (deliberate, not a real OS fault): exiting with code %d\n",
          (int)getpid(), SIMULATED_EXIT_CODE);
      fflush(stdout);
      _exit(SIMULATED_EXIT_CODE);
    }

    printf("  [worker pid=%d] finished normally\n", (int)getpid());
    fflush(stdout);
    _exit(0);
  }

  printf("[parent pid=%d] forked worker pid=%d, waiting with waitpid()\n", (int)getpid(), (int)pid);

  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("waitpid");
      return 1;
    }
  }

  if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);
    if (code == 0) {
      printf("[parent] worker %d completed normally (exit code 0)\n", (int)pid);
      return simulateFailure ? 1 : 0;
    }
    printf("[parent] worker %d FAILED: exit code %d%s\n", (int)pid, code,
        simulateFailure ? " (this failure was SIMULATED)" : "");
    return (simulateFailure && code == SIMULATED_EXIT_CODE) ? 0 : 1;
  }

  if (WIFSIGNALED(status)) {
    printf("[parent] worker %d FAILED: terminated by signal %d\n", (int)pid, WTERMSIG(status));
    return 1;
  }

  printf("[parent] worker %d ended in an unexpected state\n", (int)pid);
  return 1;
}

int failure(void) {
  return runWorker(1);
}

int success(void) {
  return runWorker(0);
}

int main(void) {
  int n;
  printf("enter case:\n");
  printf("1-> success\n");
  printf("2-> failure\n");
  if (scanf("%d", &n) != 1) {
    printf("invalid\n");
    return 1;
  }
  switch (n) {
    case 1:
      return success();
    case 2:
      return failure();
    default:
      printf("invalid\n");
      return 1;
  }
}
