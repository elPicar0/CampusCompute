#!/usr/bin/env bash
# Automated checks for campus_os. Run from the project root: make test
cd "$(dirname "$0")/.." || exit 1
BIN=./campus_os
pass=0; fail=0

[ -x "$BIN" ] || { echo "build first: make"; exit 1; }

# check <name> <expected exit code> <grep -E pattern> <menu input>
# feeds the input to the menu program (30 s limit, so a hang counts as a failure)
check() {
  local name=$1 want=$2 pattern=$3 input=$4 out rc
  out=$(printf "$input" | timeout 30 $BIN 2>&1); rc=$?
  if [ "$rc" -ne "$want" ]; then
    echo "FAIL: $name (exit $rc, wanted $want)"; fail=$((fail+1)); return
  fi
  if [ -n "$pattern" ] && ! grep -Eq -- "$pattern" <<<"$out"; then
    echo "FAIL: $name (output missing /$pattern/)"; fail=$((fail+1)); return
  fi
  echo "PASS: $name"; pass=$((pass+1))
}

# same, for command-line arguments instead of menu input
checkArgs() {
  local name=$1 want=$2 pattern=$3; shift 3
  local out rc
  out=$(timeout 60 $BIN "$@" 2>&1); rc=$?
  if [ "$rc" -ne "$want" ]; then
    echo "FAIL: $name (exit $rc, wanted $want)"; fail=$((fail+1)); return
  fi
  if [ -n "$pattern" ] && ! grep -Eq -- "$pattern" <<<"$out"; then
    echo "FAIL: $name (output missing /$pattern/)"; fail=$((fail+1)); return
  fi
  echo "PASS: $name"; pass=$((pass+1))
}

RR='1\n5\ndata/jobs_rr_check.txt\n0\n'       # load the hand-checkable job set
DEF='1\n4\n0\n'                              # load the built-in sample jobs

# scheduling
check "FCFS runs in arrival order"        0 "Order: J1 -> J2 -> J3$"                         "${RR}2\n1\n0\n0\n0\n"
check "FCFS completion/TAT/WT correct"    0 "^3 +2 +1 +1 +COMPLETED +9 +7 +6"                "${RR}2\n1\n0\n0\n0\n"
check "FCFS shows tie-break + averages"   0 "Tie-breaking rule.*lower job ID"                "${RR}2\n1\n0\n0\n0\n"
check "RR order (quantum 2)"              0 "Order: J1 -> J2 -> J3 -> J1 -> J2 -> J1"        "${RR}2\n2\n2\n0\n0\n0\n"
check "RR completion times correct"       0 "^1 +0 +5 +1 +COMPLETED +9 +9 +4"                "${RR}2\n2\n2\n0\n0\n0\n"
check "RR prints its quantum"             0 "time quantum = 2"                                "${RR}2\n2\n2\n0\n0\n0\n"
check "RR quantum 0 is rejected (no hang)" 0 "quantum must be a positive integer"            "${RR}2\n2\n0\n0\n0\n"
check "Priority order and tie-break"      0 "Order: J1 -> J2 -> J5 -> J4 -> J3"              "${DEF}2\n3\n0\n0\n0\n"
check "Idle gap shown"                    0 "IDLE"                                           "1\n5\ndata/jobs_idle.txt\n0\n2\n1\n0\n0\n0\n"
check "Policy comparison"                 0 "Lowest average waiting time"                    "${DEF}2\n4\n2\n0\n0\n0\n"
check "No jobs loaded"                    0 "no jobs loaded"                                 "2\n1\n0\n0\n0\n"

# input validation (FR8)
check "Malformed job file reported"       0 "line 3"                                         "1\n5\ndata/jobs_bad.txt\n0\n0\n0\n"
check "Missing job file reported"         0 "cannot open"                                    "1\n5\ndata/nope.txt\n0\n0\n0\n"
check "Bad burst rejected"                0 "CPU burst must be"                              "1\n1\n1 0 0 1\n0\n0\n"
check "Duplicate job ID rejected"         0 "already exists"                                 "1\n1\n1 0 5 1\n1\n1 2 3 1\n0\n0\n"
check "Letters in a submenu (no hang)"    0 "Invalid input"                                  "2\nabc\n0\n0\n"
check "Invalid main menu choice"          0 "Invalid choice"                                 "42\n0\n"
check "End of input exits cleanly"        0 "End of input"                                   "2\n"
checkArgs "Unknown command-line argument" 2 "unknown command"                                frobnicate

# logger
check "Logger writes every message"       0 "has 24 lines \(expected 24\) -> OK"             "3\n0\n"
check "Logger shows full buffer waiting"  0 "buffer FULL"                                    "3\n0\n"
check "Logger threads terminate"          0 "terminated cleanly"                             "3\n0\n"

# IPC, Banker's, workers, limits, cleanup
check "IPC exchange and cleanup"          0 "Job 1 turnaround time: 8"                       "4\n0\n"
check "IPC uses a real scheduled job"     0 "from the last scheduling run"                   "${RR}2\n1\n0\n4\n0\n"
check "Banker safe sequence"              0 "Safe sequence: P2 -> P4 -> P5 -> P1 -> P3"      "5\n1\n0\n0\n"
check "Banker unsafe state"               0 "UNSAFE state"                                   "5\n2\n0\n0\n"
check "Banker unsafe state is valid"      0 "Available: <1 0 0>"                             "5\n2\n0\n0\n"
check "Worker normal"                     0 "completed normally"                             "6\n1\n0\n0\n"
check "Worker simulated failure"          0 "SIMULATED"                                      "6\n2\n0\n0\n"
check "CPU and memory limits"             0 "killed by OS signal 24.*|malloc\\(\\) failed"   "7\n0\n"
check "CPU limit observed"                0 "CPU time limit exceeded"                        "7\n0\n"
check "Memory limit observed"             0 "malloc\\(\\) failed as expected"                "7\n0\n"
check "Cleanup verified after limits"     0 "ALL CLEAN"                                      "7\n8\n0\n"

# integrated demo
checkArgs "Integrated demo"               0 "7 of 7 stages passed"                           demo
checkArgs "Verify after demo"             0 "ALL CLEAN"                                      verify

[ "$(wc -l < campus.log)" -eq 24 ] && { echo "PASS: campus.log has 24 entries"; pass=$((pass+1)); } \
                                   || { echo "FAIL: campus.log does not have 24 entries"; fail=$((fail+1)); }

echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
