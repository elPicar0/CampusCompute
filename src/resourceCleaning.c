#include "../include/resClean.h"
#include "../include/campus.h"

#include <dirent.h>
#include <signal.h>

// ---------- cleanup ----------

// quiet safety net registered with atexit(): remove the temp file, reap children
static void quietCleanup(void) {
  unlink(TEMP_FILE);
  while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

// explicit cleanup: delete the temp file and reap any child still waiting to be collected
void cleanup_resources(void) {
  printf("FR7 Cleanup:\n");
  if (unlink(TEMP_FILE) == 0) printf("[Cleanup] Deleted '%s'.\n", TEMP_FILE);
  else printf("[Cleanup] '%s' was not present.\n", TEMP_FILE);

  int reaped = 0;
  while (waitpid(-1, NULL, WNOHANG) > 0) reaped++;
  printf("[Cleanup] Reaped %d leftover child process(es).\n", reaped);
}

// ---------- resource limits ----------

static void printLimit(const char* what, int resource) {
  struct rlimit rl;
  if (getrlimit(resource, &rl) != 0) {
    perror("getrlimit");
    return;
  }
  char soft[32], hard[32];
  if (rl.rlim_cur == RLIM_INFINITY) snprintf(soft, sizeof(soft), "unlimited");
  else snprintf(soft, sizeof(soft), "%llu", (unsigned long long)rl.rlim_cur);
  if (rl.rlim_max == RLIM_INFINITY) snprintf(hard, sizeof(hard), "unlimited");
  else snprintf(hard, sizeof(hard), "%llu", (unsigned long long)rl.rlim_max);
  printf("  %s: soft=%s hard=%s\n", what, soft, hard);
}

// current virtual memory size in bytes (Linux /proc), 0 if unknown
static unsigned long currentVmBytes(void) {
  FILE* fp = fopen("/proc/self/statm", "r");
  unsigned long pages = 0;
  if (fp != NULL) {
    if (fscanf(fp, "%lu", &pages) != 1) pages = 0;
    fclose(fp);
  }
  return pages * (unsigned long)sysconf(_SC_PAGESIZE);
}

// RLIMIT_CPU: an endless loop is stopped by the kernel after 1 s of CPU time
int demo_cpu_limit(void) {
  printf("CPU Limit (RLIMIT_CPU):\n");
  fflush(stdout);

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    struct rlimit core = {0, 0};
    setrlimit(RLIMIT_CORE, &core);           // SIGXCPU would otherwise dump a core file

    printLimit("before", RLIMIT_CPU);
    struct rlimit rl;
    if (getrlimit(RLIMIT_CPU, &rl) != 0) _exit(1);
    rl.rlim_cur = 1;                         // soft limit: 1 second of CPU time
    if (rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max) rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_CPU, &rl) != 0) {
      perror("setrlimit");
      _exit(1);
    }
    printLimit("after ", RLIMIT_CPU);
    printf("  Worker %d: CPU limit set to 1s. Starting infinite loop\n", (int)getpid());
    fflush(stdout);

    volatile unsigned long spin = 0;
    for (;;) spin++;
  }

  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("waitpid");
      return 1;
    }
  }

  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGXCPU) {
    printf("Parent: worker %d killed by OS signal %d (%s)\n", (int)pid, WTERMSIG(status), strsignal(WTERMSIG(status)));
    printf("Observed effect: the kernel stopped the endless loop once it used its 1 s of CPU time.\n");
    return 0;
  }
  printf("Parent: worker %d did NOT end by SIGXCPU (CPU limit not enforced)\n", (int)pid);
  return 1;
}

// RLIMIT_AS: malloc() fails once the address space would exceed the limit
int demo_memory_limit(void) {
  printf("Memory Limit (RLIMIT_AS):\n");
  fflush(stdout);

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    unsigned long cap = currentVmBytes() + 10UL * 1024 * 1024;   // what the process already uses + 10 MB
    if (cap < 10UL * 1024 * 1024) cap = 10UL * 1024 * 1024;

    printLimit("before", RLIMIT_AS);
    struct rlimit rl;
    if (getrlimit(RLIMIT_AS, &rl) != 0) _exit(1);
    rl.rlim_cur = cap;
    if (rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max) rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
      perror("setrlimit");
      _exit(1);
    }
    printLimit("after ", RLIMIT_AS);
    printf("  Worker %d: memory limit = current size + 10 MB. Trying to malloc 1024 MB.\n", (int)getpid());
    fflush(stdout);

    char* ptr = malloc(1024UL * 1024 * 1024);
    if (ptr == NULL) {
      printf("  [Worker %d] malloc() failed as expected: %s\n", (int)getpid(), strerror(errno));
      fflush(stdout);
      _exit(3);
    }
    printf("  [Worker %d] malloc() succeeded: limit NOT enforced\n", (int)getpid());
    fflush(stdout);
    free(ptr);
    _exit(0);
  }

  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("waitpid");
      return 1;
    }
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) == 3) {
    printf("Parent: worker %d exited with code 3 (its malloc was refused by the limit)\n", (int)pid);
    printf("Observed effect: allocations beyond the address-space limit fail with ENOMEM.\n");
    return 0;
  }
  printf("Parent: worker %d ended unexpectedly (status 0x%x)\n", (int)pid, status);
  return 1;
}

// both limit demos plus explicit cleanup of the file they use
int run_resource_demo(void) {
  static int registered = 0;
  printf("\n--- Resource Limit & Cleanup ---\n");
  if (!registered) {
    atexit(quietCleanup);
    registered = 1;
  }

  FILE* fp = fopen(TEMP_FILE, "w");
  if (fp) {
    fprintf(fp, "Temporary log data\n");
    fclose(fp);
    printf("[Init] Created '%s'.\n", TEMP_FILE);
  } else {
    perror("fopen");
  }

  int rc = 0;
  rc |= demo_cpu_limit();
  rc |= demo_memory_limit();
  cleanup_resources();
  return rc;
}

// ---------- verification: checks the real state, nothing is assumed ----------

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

int verify_cleanup(void) {
  int failures = 0;
  printf("\n=== Cleanup verification ===\n");

  if (access(TEMP_FILE, F_OK) == 0) {
    printf("[FAIL] temporary file '%s' still exists\n", TEMP_FILE);
    failures++;
  } else {
    printf("[PASS] temporary file '%s' does not exist\n", TEMP_FILE);
  }

  // IPC objects created by this program all start with "campus_"
  DIR* d = opendir("/dev/shm");
  if (d == NULL) {
    printf("[PASS] /dev/shm not available here, nothing to check\n");
  } else {
    int left = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
      if (strncmp(e->d_name, "campus_", 7) == 0 || strncmp(e->d_name, "sem.campus_", 11) == 0) {
        printf("  leftover: /dev/shm/%s\n", e->d_name);
        left++;
      }
    }
    closedir(d);
    printf("[%s] shared-memory objects and semaphores named campus_*: %d\n", left == 0 ? "PASS" : "FAIL", left);
    if (left != 0) failures++;
  }

  int st;
  pid_t w = waitpid(-1, &st, WNOHANG);
  if (w == -1 && errno == ECHILD) {
    printf("[PASS] child processes: none left (all reaped)\n");
  } else {
    printf("[FAIL] child processes: one is still running or was not reaped\n");
    failures++;
  }

  int threads = threadCount();
  if (threads < 0) {
    printf("[PASS] thread count unavailable here, skipped\n");
  } else {
    printf("[%s] threads in this process: %d (expected 1)\n", threads == 1 ? "PASS" : "FAIL", threads);
    if (threads != 1) failures++;
  }

  printf("Cleanup verification: %s\n", failures == 0 ? "ALL CLEAN" : "PROBLEMS FOUND");
  return failures == 0 ? 0 : 1;
}

int main(void) {
  int rc = run_resource_demo();
  rc |= verify_cleanup();
  return rc;
}
