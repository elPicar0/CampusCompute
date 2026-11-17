#ifndef IPC_H
#define IPC_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

typedef struct {
  int jobId;
  int arrival;
  int completion;
  int turnaround;
} sharedJob;

#endif
