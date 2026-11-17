#ifndef JOBIO_H
#define JOBIO_H

#include "job.h"

/* Loads jobs from a text file, one job per line:
 *     jobID arrivalTime burstTime priority
 * Blank lines and '#' comments are ignored. Returns number of jobs, or -1
 * after printing a message that names the file and line. */
int loadJobs(const char* path, Job* jobs, int max);

/* Built-in repeatable job set used when no file is given. Returns count. */
int defaultJobs(Job* jobs, int max);

void printJobTable(const Job* jobs, int n);

#endif
