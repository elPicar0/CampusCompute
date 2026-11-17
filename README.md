# CampusCompute (OS Level 3 Mini-Project)

One C program that implements FR1-FR8 as separate modules behind a single menu. Linux only (Ubuntu / WSL / Docker): it relies on POSIX shared memory, named semaphores, `fork` and `setrlimit`.

## Build and run
    sudo apt install build-essential     # only if gcc/make are missing
    make                                 # produces ./campus_os (compiled with -Wall -Wextra)
    ./campus_os                          # interactive menu
    ./campus_os demo                     # runs all 7 stages, prints a PASS/FAIL summary
    ./campus_os verify                   # confirms nothing was left behind
    make test                            # 36 automated checks
    make clean

## Menu and the logic behind each option
1. **Job management** - jobs are validated on entry (unique ID, arrival >= 0, burst > 0) and held in a linked list. Jobs can be added, shown, reset, or loaded from the sample set or a file.
2. **Scheduling** - all three policies are simulated on a clock that jumps over idle gaps and records them as IDLE in the timeline.
   - FCFS: picks the earliest arrival.
   - Round Robin: runs a job for min(quantum, remaining) and requeues it; the quantum must be a positive integer.
   - Priority (non-preemptive): among arrived jobs, the lowest priority number runs to completion.
   - Ties on arrival or priority go to the lower job ID.
   - Each run prints the timeline, then CT, TAT (= CT - arrival) and WT (= TAT - burst) per job, plus averages. The compare option shows all three side by side.
3. **Logger (producer-consumer)** - 3 producer threads each push 8 messages into a bounded buffer of 5; 1 consumer writes them to `campus.log`. A mutex guards the buffer, and two condition variables (not-full / not-empty) make producers wait when it is full and the consumer wait when it is empty. The consumer exits once every producer has finished and the buffer is drained; the program then checks the log has exactly 24 lines.
4. **IPC** - a parent and child share a job record through POSIX shared memory, with a named semaphore ordering the hand-off. The job comes from the last scheduling run when there is one. All objects are unlinked afterwards.
5. **Banker's algorithm** - computes Need = Maximum - Allocation, then repeatedly finds a process whose Need fits in Work and releases its Allocation. A full pass gives the safe sequence (P2 -> P4 -> P5 -> P1 -> P3); a state where no process can proceed is reported UNSAFE.
6. **Worker process** - a forked child does the work; the parent uses `waitpid` and decodes the status (normal exit, non-zero exit, or killed by signal). The failure case is deliberate and labelled SIMULATED.
7. **Resource limits** - inside a forked child, `RLIMIT_CPU` forces a SIGXCPU kill on a busy loop, and `RLIMIT_AS` makes an oversized `malloc` fail. The parent reports what it observed.
8. **Verify cleanup** - checks for leftover temp files, `campus_*` objects in `/dev/shm`, unreaped children and extra threads.
9. **Integrated demo** - runs the modules in order and reports "N of 7 stages passed".
0. Exit.

## Job file format
One job per line: `jobID arrival burst priority`. Lines starting with `#` are ignored. Lower priority number means higher priority. A malformed line is rejected with its line number and the previously loaded jobs are kept.

## Layout
`include/` headers, `src/` modules, `data/` sample job files, `tests/run_tests.sh`, `Makefile`.
