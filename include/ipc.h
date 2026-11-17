#ifndef IPC_H
#define IPC_H

#include "logger.h"

#define IPC_MAX_VALUES 64
/* Every shared-memory object / semaphore this program creates starts with
 * this prefix, so cleanup never touches anybody else's IPC objects. */
#define IPC_NAME_PREFIX "campuscompute_"

/* FR4: parent writes `values` into POSIX shared memory, signals a named
 * semaphore; a forked child reads them, computes sum/average, writes the
 * result back and signals a second named semaphore. All IPC objects are
 * unlinked before returning. Returns 0 on success. lg may be NULL. */
int runIpcDemo(const int* values, int n, Logger* lg);

/* Removes only leftovers with our prefix (from crashed earlier runs).
 * Returns number of objects removed. */
int ipcCleanupStale(void);

/* Counts leftovers with our prefix in /dev/shm (shm and sem.* files). */
int ipcCountLeftovers(int printThem);

#endif
