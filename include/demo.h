#ifndef DEMO_H
#define DEMO_H

/* FR8 / S21: runs every feature in sequence, logs events to logs/campus.log
 * through the concurrent logger, verifies cleanup and prints a PASS/FAIL
 * summary. Returns 0 if every stage passed. */
int runIntegratedDemo(void);

#endif
