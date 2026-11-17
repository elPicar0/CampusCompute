#ifndef CAMPUS_H
#define CAMPUS_H

#include <pthread.h>

// Every function below returns 0 on success (or "behaved as expected") and
// non-zero on failure, unless stated otherwise.

// jobs.c
int addJob(int id, int arrival, int burst, int priority);
void displayJob(void);
void resetJob(void);
int loadDefaultJobs(void);
int loadJobsFile(const char* path);          // returns number of jobs, -1 on error
int fcfs(void);
int roundRobin(int quantum);
int prioritySched(void);
int compareSchedulers(int quantum);
int firstCompletedJob(int* id, int* arrival, int* completion);  // 0 if one exists

// logger.c
int runLogger(void);
extern pthread_mutex_t mutex;
extern pthread_cond_t notFull;
extern pthread_cond_t notEmpty;

// ipc.c
int runIPC(void);

// banker.c
int banker_algo_success(void);               // 0 if the state was reported SAFE
int banker_algo_deadlock(void);              // 0 if the state was reported UNSAFE

// workerfailure.c
int success(void);                           // 0 if parent saw a normal exit
int failure(void);                           // 0 if parent detected the simulated failure

// resourceCleaning.c
void cleanup_resources(void);
int demo_cpu_limit(void);
int demo_memory_limit(void);
int run_resource_demo(void);
int verify_cleanup(void);

// demo.c
int runDemo(void);

#endif
