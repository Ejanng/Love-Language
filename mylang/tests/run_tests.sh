#!/usr/bin/env bash
# Runs every examples/*.my and compares its output (stdout and stderr) with the matching .expected file.
# Usage: tests/run_tests.sh [path-to-mylang-binary]
set -u
cd "$(dirname "$0")/.."
bin="${1:-./mylang}"
pass=0
fail=0
for src in examples/*.my; do
  expected="${src%.my}.expected"
  actual="$("$bin" "$src" 2>&1)"
  if [ "$actual" == "$(cat "$expected")" ]; then
    pass=$((pass + 1))
    echo "PASS $src"
  else
    fail=$((fail + 1))
    echo "FAIL $src"
    diff <(echo "$actual") "$expected" | head -n 10
  fi
done
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
