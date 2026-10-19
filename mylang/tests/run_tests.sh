#!/usr/bin/env bash
# Runs every examples/*.my on both backends (tree-walker and bytecode VM) and compares
# the output (stdout and stderr) with the matching .expected file.
# Usage: tests/run_tests.sh [path-to-mylang-binary]
set -u
cd "$(dirname "$0")/.."
bin="${1:-./mylang}"
pass=0
fail=0
for mode in tree vm; do
  flag=""
  [ "$mode" == "vm" ] && flag="--vm"
  for src in examples/*.my; do
    expected="${src%.my}.expected"
    actual="$("$bin" $flag "$src" 2>&1)"
    if [ "$actual" == "$(cat "$expected")" ]; then
      pass=$((pass + 1))
      echo "PASS [$mode] $src"
    else
      fail=$((fail + 1))
      echo "FAIL [$mode] $src"
      diff <(echo "$actual") "$expected" | head -n 10
    fi
  done
done
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
