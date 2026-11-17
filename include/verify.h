#ifndef VERIFY_H
#define VERIFY_H

/* FR7 / S20: checks that the program left nothing behind:
 *   1. no shared-memory objects or named semaphores with our prefix
 *   2. no unreaped (zombie) or still-running child processes
 *   3. only the main thread is alive
 * Prints PASS/FAIL per check. Returns 0 if everything is clean. */
int verifyCleanup(void);

#endif
