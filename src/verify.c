#include "../include/verify.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include "../include/ipc.h"

static int threadCount(void) {
  FILE* fp = fopen("/proc/self/status", "r");
  if (fp == NULL) return -1;
  char line[128];
  int n = -1;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (sscanf(line, "Threads: %d", &n) == 1) break;
  }
  fclose(fp);
  return n;
}

int verifyCleanup(void) {
  int failures = 0;
  printf("\n=== Cleanup verification ===\n");

  int left = ipcCountLeftovers(1);
  printf("[%s] IPC objects named %s* in /dev/shm: %d\n", left == 0 ? "PASS" : "FAIL", IPC_NAME_PREFIX, left);
  if (left != 0) failures++;

  int st = 0;
  pid_t w = waitpid(-1, &st, WNOHANG);
  int noChildren = (w == -1 && errno == ECHILD);
  printf("[%s] child processes: %s\n", noChildren ? "PASS" : "FAIL",
         noChildren ? "none left (all reaped)" : "a child is still running or was not reaped");
  if (!noChildren) failures++;

  int threads = threadCount();
  printf("[%s] threads in this process: %d (expected 1)\n", threads == 1 ? "PASS" : "FAIL", threads);
  if (threads != 1) failures++;

  printf("Cleanup verification: %s\n", failures == 0 ? "ALL CLEAN" : "PROBLEMS FOUND");
  return failures == 0 ? 0 : -1;
}
