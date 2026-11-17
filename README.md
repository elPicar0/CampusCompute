# CampusCompute

Command-line prototype for the OS Level 3 mini-project: job scheduling, a
concurrent logger, POSIX IPC, Banker's safety check, worker-failure detection
and resource limits. Written in C for Linux (Ubuntu / WSL Ubuntu).

## Prerequisites

- Ubuntu or WSL Ubuntu
- `gcc` and `make`  (`sudo apt install build-essential`)
- A normal `/dev/shm` (present by default; POSIX shared memory and named semaphores live there)

## Build

```
make            # builds ./campus
make clean      # removes build/, ./campus and logs/
```

## Run

```
./campus help                      # all commands
./campus sched fcfs                # also: rr [-q N], priority, compare   (optional: -f data/jobs.txt)
./campus logger [producers items capacity]
./campus ipc
./campus banker [safe|unsafe|file]
./campus worker [ok|fail|crash|pool]
./campus limits [nofile|mem|cpu|all]
./campus verify                    # confirms nothing was left behind
./campus demo                      # integrated run of everything + summary
```

Each command is independent, so every feature can be tested on its own.
Exit codes: 0 success, 1 runtime/input error, 2 usage error.

## Scheduling details

- Job fields: job ID, arrival time, CPU burst, priority, status (NEW/READY/RUNNING/DONE).
- Input file: one job per line, `jobID arrival burst priority`, `#` comments allowed
  (see `data/jobs.txt`). Without `-f` a built-in 5-job set is used.
- **Tie-breaking:** equal arrival time -> lower job ID first. For priority scheduling,
  lower number = higher priority; equal priority -> earlier arrival, then lower job ID.
- **Round Robin:** default time quantum 2 (`-q N` to change). A job that arrives during
  a time slice is queued before the preempted job.
- Idle CPU time between jobs is shown as `IDLE` in the timeline.

## Other behaviour worth knowing

- **Logger:** bounded circular buffer, one mutex, two condition variables (not-full /
  not-empty). Output goes to `logs/logger_demo.log` (`logger`) or `logs/campus.log` (`demo`).
  Shutdown: producers are joined, the logger is closed, the consumer drains the buffer and is joined.
- **IPC:** parent and a forked child exchange data through POSIX shared memory and two named
  semaphores. Object names start with `campuscompute_` and include the parent PID.
- **Workers:** created with `fork()` and reaped with `waitpid()`. Failures are **simulated**
  (the worker deliberately exits with code 3 or raises SIGTERM on itself) and are always
  labelled as such; they are not real OS faults.
- **Limits:** `setrlimit()` is applied only inside a forked child. `nofile` (RLIMIT_NOFILE)
  makes `open()` fail with EMFILE, `mem` (RLIMIT_AS) makes `malloc()` return NULL, `cpu`
  (RLIMIT_CPU) kills a busy loop with SIGXCPU after 1 CPU second.

## Cleanup

All threads are joined, all children are reaped, and all shared memory / semaphores are
unlinked before each command exits. To check:

```
./campus verify      # no leftover IPC objects, child processes or extra threads
ls /dev/shm          # nothing named campuscompute_*
```

If a run is ever killed half-way (Ctrl+C) and leaves objects behind, remove only this
program's objects with:

```
./campus cleanup     # or: make cleanup-ipc
```

This touches only names starting with `campuscompute_` / `sem.campuscompute_`; other
programs' IPC objects are never removed.

## Tests

```
make test            # runs tests/run_tests.sh (all features, invalid input, cleanup)
```

## Files

| File | Purpose |
|------|---------|
| `main.c`, `cli.c/.h` | entry point, command parsing, usage and error messages |
| `job.c/.h`, `jobio.c/.h` | job model, validation, file loader |
| `queue.c/.h` | linked-list queue (used by Round Robin) |
| `scheduler.c/.h` | FCFS, Round Robin, Priority, timeline, metrics, comparison |
| `logger.c/.h` | pthread producer-consumer logger |
| `ipc.c/.h` | shared memory + named semaphore IPC, stale-object cleanup |
| `banker.c/.h` | Banker's safety algorithm |
| `worker.c/.h` | fork/waitpid workers, simulated failures |
| `limits.c/.h` | setrlimit demonstrations |
| `verify.c/.h` | cleanup verification |
| `demo.c/.h` | integrated demonstration |
| `data/` | sample job sets and Banker's states |
| `tests/run_tests.sh` | automated checks |
