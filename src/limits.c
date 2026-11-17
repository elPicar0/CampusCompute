#include "../include/limits.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define NOFILE_SOFT 16
#define MEM_HEADROOM_BYTES (48UL * 1024 * 1024)
#define MEM_CHUNK_BYTES (4UL * 1024 * 1024)
#define CPU_LIMIT_SECONDS 1

static void printLimit(const char* what, const struct rlimit* rl) {
  char soft[32], hard[32];
  if (rl->rlim_cur == RLIM_INFINITY) snprintf(soft, sizeof(soft), "unlimited");
  else snprintf(soft, sizeof(soft), "%llu", (unsigned long long)rl->rlim_cur);
  if (rl->rlim_max == RLIM_INFINITY) snprintf(hard, sizeof(hard), "unlimited");
  else snprintf(hard, sizeof(hard), "%llu", (unsigned long long)rl->rlim_max);
  printf("    %s: soft=%s hard=%s\n", what, soft, hard);
}

/* Current virtual memory size in bytes, from /proc/self/statm. */
static unsigned long currentVmBytes(void) {
  FILE* fp = fopen("/proc/self/statm", "r");
  unsigned long pages = 0;
  if (fp != NULL) {
    if (fscanf(fp, "%lu", &pages) != 1) pages = 0;
    fclose(fp);
  }
  return pages * (unsigned long)sysconf(_SC_PAGESIZE);
}

/* ---- child bodies: each returns the child's exit code (0 = expected effect seen) ---- */

static int childNofile(void) {
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  printf("  [child pid=%d] before:\n", (int)getpid());
  printLimit("RLIMIT_NOFILE", &rl);

  rl.rlim_cur = NOFILE_SOFT;                         /* lower the soft limit only */
  if (rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max) rl.rlim_cur = rl.rlim_max;
  if (setrlimit(RLIMIT_NOFILE, &rl) != 0) { perror("  setrlimit"); return 1; }
  getrlimit(RLIMIT_NOFILE, &rl);
  printf("  [child] after setrlimit:\n");
  printLimit("RLIMIT_NOFILE", &rl);

  int fds[1024], opened = 0, err = 0;
  for (; opened < 1024; opened++) {
    fds[opened] = open("/dev/null", O_RDONLY);
    if (fds[opened] < 0) { err = errno; break; }
  }
  printf("  [child] opened %d extra files, then open() failed with errno=%d (%s)\n", opened, err, strerror(err));
  for (int i = 0; i < opened; i++) close(fds[i]);
  printf("  [child] observed effect: the process cannot hold more than %d descriptors\n", (int)rl.rlim_cur);
  return err == EMFILE ? 0 : 1;
}

static int childMem(void) {
  struct rlimit rl;
  getrlimit(RLIMIT_AS, &rl);
  printf("  [child pid=%d] before:\n", (int)getpid());
  printLimit("RLIMIT_AS (address space, bytes)", &rl);

  unsigned long limit = currentVmBytes() + MEM_HEADROOM_BYTES;   /* current size + 48 MiB */
  if (rl.rlim_max != RLIM_INFINITY && limit > rl.rlim_max) limit = rl.rlim_max;
  rl.rlim_cur = limit;
  if (setrlimit(RLIMIT_AS, &rl) != 0) { perror("  setrlimit"); return 1; }
  getrlimit(RLIMIT_AS, &rl);
  printf("  [child] after setrlimit:\n");
  printLimit("RLIMIT_AS (address space, bytes)", &rl);

  void* chunks[256];
  int n = 0, err = 0;
  for (; n < 256; n++) {
    chunks[n] = malloc(MEM_CHUNK_BYTES);
    if (chunks[n] == NULL) { err = errno; break; }
  }
  printf("  [child] allocated %d chunks of 4 MiB (%d MiB), then malloc() returned NULL (errno=%d: %s)\n",
         n, n * 4, err, strerror(err));
  for (int i = 0; i < n; i++) free(chunks[i]);
  printf("  [child] observed effect: allocation fails once the address-space limit is reached\n");
  return (n < 256 && err == ENOMEM) ? 0 : 1;
}

static int childCpu(void) {
  struct rlimit core = {0, 0};
  setrlimit(RLIMIT_CORE, &core);                     /* SIGXCPU would otherwise dump core */

  struct rlimit rl;
  getrlimit(RLIMIT_CPU, &rl);
  printf("  [child pid=%d] before:\n", (int)getpid());
  printLimit("RLIMIT_CPU (seconds)", &rl);
  rl.rlim_cur = CPU_LIMIT_SECONDS;
  if (rl.rlim_max != RLIM_INFINITY && rl.rlim_cur > rl.rlim_max) rl.rlim_cur = rl.rlim_max;
  if (setrlimit(RLIMIT_CPU, &rl) != 0) { perror("  setrlimit"); return 1; }
  getrlimit(RLIMIT_CPU, &rl);
  printf("  [child] after setrlimit:\n");
  printLimit("RLIMIT_CPU (seconds)", &rl);
  printf("  [child] entering an endless busy loop; the kernel should stop it after ~%d CPU second\n", CPU_LIMIT_SECONDS);
  fflush(stdout);

  volatile unsigned long x = 0;
  for (;;) x++;
  return 1;                                          /* not reached */
}

int runLimitDemo(LimitKind kind, Logger* lg) {
  static const char* names[] = {"RLIMIT_NOFILE (open files)", "RLIMIT_AS (memory)", "RLIMIT_CPU (CPU seconds)"};
  printf("\n--- %s ---\n", names[kind]);
  printf("The limit is applied inside a forked child; the parent is not restricted.\n");
  fflush(stdout);

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }
  if (pid == 0) {
    int rc = 1;
    switch (kind) {
      case LIMIT_NOFILE: rc = childNofile(); break;
      case LIMIT_MEM:    rc = childMem();    break;
      case LIMIT_CPU:    rc = childCpu();    break;
    }
    fflush(stdout);
    _exit(rc);
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) { perror("waitpid"); return -1; }
  }

  int ok;
  if (kind == LIMIT_CPU) {
    ok = WIFSIGNALED(status) && WTERMSIG(status) == SIGXCPU;
    if (WIFSIGNALED(status))
      printf("[parent] child terminated by signal %d (%s) -> CPU limit enforced: %s\n",
             WTERMSIG(status), strsignal(WTERMSIG(status)), ok ? "YES" : "NO");
    else
      printf("[parent] child exited normally with %d -> CPU limit NOT enforced\n", WEXITSTATUS(status));
  } else {
    ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    printf("[parent] child exit status %d -> limit effect observed: %s\n",
           WIFEXITED(status) ? WEXITSTATUS(status) : -1, ok ? "YES" : "NO");
  }
  loggerLog(lg, "LIMITS", "%s: effect %s", names[kind], ok ? "observed" : "NOT observed");
  return ok ? 0 : -1;
}
