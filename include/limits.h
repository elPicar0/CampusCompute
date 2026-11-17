#ifndef LIMITS_H
#define LIMITS_H

#include "logger.h"

/* FR7: each demo forks a child, applies a limit with setrlimit() ONLY in that
 * child (the parent is never limited), and shows the effect. */
typedef enum {
  LIMIT_NOFILE,   /* RLIMIT_NOFILE: open() starts failing with EMFILE            */
  LIMIT_MEM,      /* RLIMIT_AS:     malloc() starts returning NULL (ENOMEM)      */
  LIMIT_CPU       /* RLIMIT_CPU:    busy loop is killed by SIGXCPU after 1 s CPU */
} LimitKind;

/* Returns 0 if the limit was applied and the expected effect was observed. */
int runLimitDemo(LimitKind kind, Logger* lg);

#endif
