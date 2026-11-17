#!/usr/bin/env bash
# Automated checks for CampusCompute. Run from the project root: make test
cd "$(dirname "$0")/.." || exit 1
BIN=./campus
pass=0; fail=0

# check <name> <expected exit code> <grep pattern or ""> <command...>
check() {
  local name=$1 want=$2 pattern=$3; shift 3
  local out rc
  out=$("$@" 2>&1); rc=$?
  if [ "$rc" -ne "$want" ]; then
    echo "FAIL: $name (exit $rc, wanted $want)"; fail=$((fail+1)); return
  fi
  if [ -n "$pattern" ] && ! grep -Eq -- "$pattern" <<<"$out"; then
    echo "FAIL: $name (output missing /$pattern/)"; fail=$((fail+1)); return
  fi
  echo "PASS: $name"; pass=$((pass+1))
}

[ -x "$BIN" ] || { echo "build first: make"; exit 1; }

check "help"                        0 "Usage: campus"                       $BIN help
check "fcfs default"                0 "Average: completion"                 $BIN sched fcfs
check "fcfs idle gap"               0 "IDLE"                                $BIN sched fcfs -f data/jobs_idle.txt
check "rr hand-checked order"       0 "Order: J1 -> J2 -> J3 -> J1 -> J2 -> J1" $BIN sched rr -q 2 -f data/jobs_rr_check.txt
check "rr completion times"         0 "J3.* 5 .*|J1 .* 9 "                  $BIN sched rr -q 2 -f data/jobs_rr_check.txt
check "priority tie-break rule"     0 "ties -> earlier arrival"             $BIN sched priority
check "priority order"              0 "Order: J1 -> J2 -> J5 -> J4 -> J3"  $BIN sched priority
check "compare"                     0 "Lowest average waiting time"         $BIN sched compare
check "bad quantum"                 2 "invalid quantum"                     $BIN sched rr -q 0
check "bad policy"                  2 "unknown scheduling policy"           $BIN sched lifo
check "malformed job file"          1 "line 3"                              $BIN sched fcfs -f data/jobs_bad.txt
check "missing job file"            1 "cannot open"                         $BIN sched fcfs -f data/nope.txt
check "banker safe"                 0 "Safe sequence: P1 -> P3 -> P4 -> P0 -> P2" $BIN banker safe
check "banker unsafe"               0 "UNSAFE state"                        $BIN banker unsafe
check "banker file safe"            0 "SAFE state"                          $BIN banker data/banker_safe.txt
check "banker file unsafe"          0 "UNSAFE state"                        $BIN banker data/banker_unsafe.txt
check "logger"                      0 "has 12 lines \(expected 12\)"        $BIN logger
check "logger custom args"          0 "has 10 lines \(expected 10\)"        $BIN logger 2 5 2
check "logger bad args"             2 "producers must be"                   $BIN logger 0 5 2
check "ipc"                         0 "MATCH"                               $BIN ipc
check "worker ok"                   0 "COMPLETED NORMALLY"                  $BIN worker ok
check "worker simulated exit"       0 "exit code 3.*SIMULATED"              $BIN worker fail
check "worker simulated crash"      0 "Terminated.*SIMULATED"               $BIN worker crash
check "worker pool"                 0 "2 completed normally, 2 failed"      $BIN worker pool
check "limit nofile"                0 "EMFILE|Too many open files"          $BIN limits nofile
check "limit mem"                   0 "malloc\(\) returned NULL"            $BIN limits mem
check "limit cpu"                   0 "CPU limit enforced: YES"             $BIN limits cpu
check "invalid command"             2 "unknown command"                     $BIN frobnicate
check "no args prints usage"        2 "Usage: campus"                       $BIN
check "verify clean"                0 "ALL CLEAN"                           $BIN verify
check "integrated demo"             0 "8 of 8 stages passed"                $BIN demo
check "verify after demo"           0 "ALL CLEAN"                           $BIN verify

[ -s logs/campus.log ] && { echo "PASS: log file created"; pass=$((pass+1)); } \
                       || { echo "FAIL: logs/campus.log missing"; fail=$((fail+1)); }

echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
