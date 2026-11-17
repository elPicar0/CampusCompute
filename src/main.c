#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/campus.h"

// ---------- safe input helpers ----------
// All keyboard input is read as a whole line and checked, so letters or empty
// lines produce an error message instead of an endless loop.

// reads one line; at end of input the program exits cleanly
static void readLine(char* buf, size_t size) {
  if (fgets(buf, (int)size, stdin) == NULL) {
    printf("\nEnd of input, exiting.\n");
    exit(0);
  }
}

// 1 = got an integer, 0 = invalid input (message printed)
static int askInt(const char* prompt, int* out) {
  char line[128];
  printf("%s", prompt);
  fflush(stdout);
  readLine(line, sizeof(line));

  char* end;
  errno = 0;
  long v = strtol(line, &end, 10);
  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
  if (end == line || *end != '\0' || errno != 0 || v < -1000000 || v > 1000000) {
    fprintf(stderr, "Invalid input: please enter a whole number.\n");
    return 0;
  }
  *out = (int)v;
  return 1;
}

// reads n integers from one line; 1 = ok, 0 = invalid (message printed)
static int askInts(const char* prompt, int* vals, int n) {
  char line[256];
  printf("%s", prompt);
  fflush(stdout);
  readLine(line, sizeof(line));

  char* p = line;
  for (int i = 0; i < n; i++) {
    char* end;
    errno = 0;
    long v = strtol(p, &end, 10);
    if (end == p || errno != 0 || v < -1000000 || v > 1000000) {
      fprintf(stderr, "Invalid input: expected %d whole numbers separated by spaces.\n", n);
      return 0;
    }
    vals[i] = (int)v;
    p = end;
  }
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (*p != '\0') {
    fprintf(stderr, "Invalid input: expected exactly %d numbers.\n", n);
    return 0;
  }
  return 1;
}

static void printUsage(const char* prog) {
  printf("Usage: %s [demo | verify | help]\n", prog);
  printf("  (no argument)  interactive menu\n");
  printf("  demo           run every feature once and print a summary\n");
  printf("  verify         check that no files, threads, children or IPC objects are left behind\n");
  printf("  help           this message\n");
}

// ---------- sub-menus ----------

static void jobMenu(void) {
  for (;;) {
    int option;
    printf("\n--- Job Management ---\n");
    printf("1. Add job\n");
    printf("2. Display jobs\n");
    printf("3. Reset jobs\n");
    printf("4. Load built-in sample jobs\n");
    printf("5. Load jobs from a file\n");
    printf("0. Back to main menu\n");
    if (!askInt("Enter option: ", &option)) continue;

    if (option == 1) {
      int v[4];
      if (askInts("Enter jobID, AT, BT, Priority: ", v, 4)) {
        if (addJob(v[0], v[1], v[2], v[3]) == 0) printf("Job %d added\n", v[0]);
      }
    } else if (option == 2) {
      displayJob();
    } else if (option == 3) {
      resetJob();
    } else if (option == 4) {
      if (loadDefaultJobs() == 0) {
        printf("Loaded the built-in sample jobs\n");
        displayJob();
      }
    } else if (option == 5) {
      char path[256];
      printf("File name: ");
      fflush(stdout);
      readLine(path, sizeof(path));
      path[strcspn(path, "\r\n")] = '\0';
      int n = loadJobsFile(path);
      if (n > 0) {
        printf("Loaded %d jobs from %s\n", n, path);
        displayJob();
      }
    } else if (option == 0) {
      return;
    } else {
      fprintf(stderr, "Invalid option: choose 0-5.\n");
    }
  }
}

static void scheduleMenu(void) {
  for (;;) {
    int option, quantum;
    printf("\n--- CPU Scheduling ---\n");
    printf("1. FCFS\n");
    printf("2. Round Robin\n");
    printf("3. Priority Scheduling (non-preemptive)\n");
    printf("4. Compare all three policies\n");
    printf("0. Back to main menu\n");
    if (!askInt("Enter option: ", &option)) continue;

    if (option == 1) {
      fcfs();
    } else if (option == 2) {
      if (askInt("Enter Quantum: ", &quantum)) roundRobin(quantum);
    } else if (option == 3) {
      prioritySched();
    } else if (option == 4) {
      if (askInt("Enter Quantum for Round Robin: ", &quantum)) compareSchedulers(quantum);
    } else if (option == 0) {
      return;
    } else {
      fprintf(stderr, "Invalid option: choose 0-4.\n");
    }
  }
}

static void bankerMenu(void) {
  for (;;) {
    int option;
    printf("\n--- Banker's Algorithm ---\n");
    printf("1. Safe state case\n");
    printf("2. Deadlock / unsafe case\n");
    printf("0. Back to main menu\n");
    if (!askInt("Enter option: ", &option)) continue;

    if (option == 1) banker_algo_success();
    else if (option == 2) banker_algo_deadlock();
    else if (option == 0) return;
    else fprintf(stderr, "Invalid option: choose 0-2.\n");
  }
}

static void workerMenu(void) {
  for (;;) {
    int option;
    printf("\n--- Worker Error Detection ---\n");
    printf("1. Success case\n");
    printf("2. Failure case (simulated)\n");
    printf("0. Back to main menu\n");
    if (!askInt("Enter option: ", &option)) continue;

    if (option == 1) success();
    else if (option == 2) failure();
    else if (option == 0) return;
    else fprintf(stderr, "Invalid option: choose 0-2.\n");
  }
}

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IOLBF, 0);          // line-buffered so parent/child output interleaves cleanly

  if (argc == 2 && strcmp(argv[1], "demo") == 0) return runDemo();
  if (argc == 2 && strcmp(argv[1], "verify") == 0) return verify_cleanup();
  if (argc == 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 ||
                    strcmp(argv[1], "-h") == 0)) {
    printUsage(argv[0]);
    return 0;
  }
  if (argc > 1) {
    fprintf(stderr, "error: unknown command '%s'\n", argv[1]);
    printUsage(argv[0]);
    return 2;
  }

  for (;;) {
    int choice;
    printf("\n=== MENU ===\n");
    printf("1. Job management and process lifecycle\n");
    printf("2. CPU scheduling\n");
    printf("3. Activity logger\n");
    printf("4. IPC (shared memory & semaphore)\n");
    printf("5. Resource safety analysis\n");
    printf("6. Worker failure detection\n");
    printf("7. Resource limit and cleanup\n");
    printf("8. Verify cleanup\n");
    printf("9. Run integrated demonstration\n");
    printf("0. Exit\n");
    if (!askInt("Enter choice: ", &choice)) continue;

    if (choice == 0) {
      pthread_mutex_destroy(&mutex);
      pthread_cond_destroy(&notFull);
      pthread_cond_destroy(&notEmpty);
      printf("Exiting\n");
      return 0;
    }

    if (choice == 1) jobMenu();
    else if (choice == 2) scheduleMenu();
    else if (choice == 3) {
      printf("\n--- Activity Logger ---\n");
      runLogger();
    }
    else if (choice == 4) {
      printf("\n--- IPC Demo ---\n");
      runIPC();
    }
    else if (choice == 5) bankerMenu();
    else if (choice == 6) workerMenu();
    else if (choice == 7) run_resource_demo();
    else if (choice == 8) verify_cleanup();
    else if (choice == 9) runDemo();
    else fprintf(stderr, "Invalid choice. Pick 0-9 only.\n");
  }
}
