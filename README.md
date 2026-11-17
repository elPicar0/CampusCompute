# CampusCompute (OS Level 3 Mini-Project)

Single C program covering FR1-FR8. Linux only (Ubuntu / WSL / Docker); it uses POSIX shared memory, named semaphores, fork and setrlimit.

## Build and run
    sudo apt install build-essential     # if gcc/make are missing
    make                                 # builds ./campus_os (-Wall -Wextra, no warnings)
    ./campus_os                          # interactive menu
    ./campus_os demo                     # integrated demo of every module (7 stages, PASS/FAIL summary)
    ./campus_os verify                   # check nothing is left behind (/dev/shm, children, threads)
    make test                            # 36 automated checks
    make clean

## Menu
1 Job management (add / display / reset / load sample / load file)
2 Scheduling (FCFS, Round Robin with quantum, Priority non-preemptive, compare)
3 Producer-consumer logger (3 producers, 1 consumer, buffer of 5, writes campus.log)
4 IPC: shared memory + named semaphore
5 Banker's algorithm (safe and unsafe cases)
6 Worker process (normal / simulated failure, clearly labelled as simulated)
7 Resource limits (CPU via RLIMIT_CPU, memory via RLIMIT_AS) in a forked child
8 Verify cleanup
9 Integrated demo
0 Exit

## Job file format
`jobID arrival burst priority` per line, `#` comments allowed. Lower priority number = higher priority. Ties are broken by lower job ID.

## Layout
include/ headers, src/ modules, data/ sample job files, tests/run_tests.sh, Makefile.
