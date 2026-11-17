#include "../include/ipc.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  int count;
  int values[IPC_MAX_VALUES];
  long sum;
  double average;
  int consumerPid;
} IpcPayload;

#define RESULT_TIMEOUT_SEC 5

// sem_wait that survives signal interruptions
static int semWaitRetry(sem_t* s) {
  while (sem_wait(s) != 0) {
    if (errno != EINTR) return -1;
  }
  return 0;
}

// Cooperating process #2: opens the same objects BY NAME.
static int consumerProcess(const char* shmName, const char* semDataName, const char* semResultName) {
  int rc = 1;
  int fd = shm_open(shmName, O_RDWR, 0);
  if (fd < 0) { perror("[ipc child] shm_open"); return 1; }
  IpcPayload* p = mmap(NULL, sizeof(IpcPayload), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (p == MAP_FAILED) { perror("[ipc child] mmap"); return 1; }

  sem_t* semData = sem_open(semDataName, 0);
  sem_t* semResult = sem_open(semResultName, 0);
  if (semData == SEM_FAILED || semResult == SEM_FAILED) {
    perror("[ipc child] sem_open");
    goto out;
  }

  printf("[ipc child  pid=%d] waiting on semaphore for data...\n", (int)getpid());
  if (semWaitRetry(semData) != 0) { perror("[ipc child] sem_wait"); goto out; }

  long sum = 0;
  for (int i = 0; i < p->count; i++) sum += p->values[i];
  p->sum = sum;
  p->average = p->count > 0 ? (double)sum / p->count : 0.0;
  p->consumerPid = (int)getpid();
  printf("[ipc child  pid=%d] read %d values from shared memory, sum=%ld average=%.2f\n",
         (int)getpid(), p->count, sum, p->average);

  if (sem_post(semResult) != 0) { perror("[ipc child] sem_post"); goto out; }
  rc = 0;

out:
  if (semData != SEM_FAILED) sem_close(semData);
  if (semResult != SEM_FAILED) sem_close(semResult);
  munmap(p, sizeof(IpcPayload));
  fflush(stdout);
  return rc;
}

int runIpcDemo(const int* values, int n, Logger* lg) {
  if (values == NULL || n <= 0 || n > IPC_MAX_VALUES) {
    fprintf(stderr, "error: ipc demo needs 1..%d values\n", IPC_MAX_VALUES);
    return -1;
  }

  char shmName[64], semDataName[64], semResultName[64];
  int pid = (int)getpid();
  snprintf(shmName, sizeof(shmName), "/" IPC_NAME_PREFIX "shm_%d", pid);
  snprintf(semDataName, sizeof(semDataName), "/" IPC_NAME_PREFIX "semdata_%d", pid);
  snprintf(semResultName, sizeof(semResultName), "/" IPC_NAME_PREFIX "semresult_%d", pid);

  int rc = -1;
  int fd = -1, shmCreated = 0, dataCreated = 0, resultCreated = 0;
  IpcPayload* p = MAP_FAILED;
  sem_t *semData = SEM_FAILED, *semResult = SEM_FAILED;
  pid_t child = -1;

  fd = shm_open(shmName, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (fd < 0) { perror("[ipc] shm_open"); goto cleanup; }
  shmCreated = 1;
  if (ftruncate(fd, (off_t)sizeof(IpcPayload)) != 0) { perror("[ipc] ftruncate"); goto cleanup; }
  p = mmap(NULL, sizeof(IpcPayload), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) { perror("[ipc] mmap"); goto cleanup; }
  close(fd);
  fd = -1;
  memset(p, 0, sizeof(IpcPayload));

  semData = sem_open(semDataName, O_CREAT | O_EXCL, 0600, 0);
  if (semData == SEM_FAILED) { perror("[ipc] sem_open(data)"); goto cleanup; }
  dataCreated = 1;
  semResult = sem_open(semResultName, O_CREAT | O_EXCL, 0600, 0);
  if (semResult == SEM_FAILED) { perror("[ipc] sem_open(result)"); goto cleanup; }
  resultCreated = 1;

  printf("[ipc parent pid=%d] created shared memory %s (%zu bytes)\n", pid, shmName, sizeof(IpcPayload));
  printf("[ipc parent pid=%d] created named semaphores %s and %s\n", pid, semDataName, semResultName);
  loggerLog(lg, "IPC", "created shm %s and 2 named semaphores", shmName);

  fflush(stdout);                                    // avoid duplicated buffered output in the child
  child = fork();
  if (child < 0) { perror("[ipc] fork"); goto cleanup; }
  if (child == 0) {
    int crc = consumerProcess(shmName, semDataName, semResultName);
    fflush(stdout);
    _exit(crc);                                      // never fall into the parent's cleanup 
  }

  // parent = producer
  p->count = n;
  long expectedSum = 0;
  for (int i = 0; i < n; i++) {
    p->values[i] = values[i];
    expectedSum += values[i];
  }
  printf("[ipc parent pid=%d] wrote %d values to shared memory:", pid, n);
  for (int i = 0; i < n; i++) printf(" %d", values[i]);
  printf("\n[ipc parent pid=%d] posting data-ready semaphore\n", pid);
  if (sem_post(semData) != 0) { perror("[ipc] sem_post"); goto cleanup; }

  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += RESULT_TIMEOUT_SEC;
  int w;
  while ((w = sem_timedwait(semResult, &deadline)) != 0 && errno == EINTR) {}
  if (w != 0) {
    fprintf(stderr, "[ipc parent] no result from child (%s)\n", strerror(errno));
    goto cleanup;
  }

  double expectedAvg = (double)expectedSum / n;
  int match = (p->sum == expectedSum) && (p->average == expectedAvg);
  printf("[ipc parent pid=%d] result from child pid=%d: sum=%ld average=%.2f (expected sum=%ld average=%.2f) -> %s\n",
         pid, p->consumerPid, p->sum, p->average, expectedSum, expectedAvg, match ? "MATCH" : "MISMATCH");
  loggerLog(lg, "IPC", "child pid=%d returned sum=%ld avg=%.2f (%s)", p->consumerPid, p->sum, p->average,
            match ? "match" : "mismatch");

  int status = 0;
  if (waitpid(child, &status, 0) == child) {
    child = -1;
    if (match && WIFEXITED(status) && WEXITSTATUS(status) == 0) rc = 0;
  }

cleanup:
  if (child > 0) {                                   // child still around after an error
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);
  }
  if (p != MAP_FAILED) munmap(p, sizeof(IpcPayload));
  if (fd >= 0) close(fd);
  if (semData != SEM_FAILED) sem_close(semData);
  if (semResult != SEM_FAILED) sem_close(semResult);
  if (dataCreated) sem_unlink(semDataName);
  if (resultCreated) sem_unlink(semResultName);
  if (shmCreated) shm_unlink(shmName);
  if (shmCreated)
    printf("[ipc parent pid=%d] cleanup: unmapped, unlinked %s and both semaphores\n", pid, shmName);
  loggerLog(lg, "IPC", "cleanup done (shm and semaphores unlinked)");
  return rc;
}

// stale object handling (only our prefix)

static int isOurShm(const char* name) {
  return strncmp(name, IPC_NAME_PREFIX, strlen(IPC_NAME_PREFIX)) == 0;
}
static int isOurSem(const char* name) {
  return strncmp(name, "sem." IPC_NAME_PREFIX, strlen("sem." IPC_NAME_PREFIX)) == 0;
}

int ipcCountLeftovers(int printThem) {
  DIR* d = opendir("/dev/shm");
  if (d == NULL) return 0;                           // nothing to inspect
  int count = 0;
  struct dirent* e;
  while ((e = readdir(d)) != NULL) {
    if (isOurShm(e->d_name) || isOurSem(e->d_name)) {
      count++;
      if (printThem) printf("  leftover: /dev/shm/%s\n", e->d_name);
    }
  }
  closedir(d);
  return count;
}

int ipcCleanupStale(void) {
  DIR* d = opendir("/dev/shm");
  if (d == NULL) return 0;
  int removed = 0;
  struct dirent* e;
  char name[300];
  while ((e = readdir(d)) != NULL) {
    if (isOurShm(e->d_name)) {
      snprintf(name, sizeof(name), "/%s", e->d_name);
      if (shm_unlink(name) == 0) { printf("removed shared memory %s\n", name); removed++; }
    } else if (isOurSem(e->d_name)) {
      snprintf(name, sizeof(name), "/%s", e->d_name + 4);   // strip "sem."
      if (sem_unlink(name) == 0) { printf("removed semaphore %s\n", name); removed++; }
    }
  }
  closedir(d);
  return removed;
}
