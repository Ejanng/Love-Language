#!/usr/bin/env bash
# Runs every example program and compares its output (stdout + stderr)
# with the matching .expected file.
#
# Usage: tests/run_tests.sh [path/to/rene-executable]

EXT="rene"  # source-file extension, without the dot (see docs/CHANGING_FILE_EXTENSION.md)

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [ -n "$1" ]; then
    RENE="$1"
else
    # Single-config generators (Make, Ninja) vs. multi-config ones (Visual Studio).
    for candidate in "$ROOT/build/rene" "$ROOT/build/rene.exe" "$ROOT/build/Release/rene.exe"; do
        [ -x "$candidate" ] && RENE="$candidate" && break
    done
fi
[ -n "$RENE" ] || { echo "rene executable not found; build it first or pass its path"; exit 1; }

pass=0
fail=0
for src in "$ROOT"/examples/*."$EXT"; do
    expected="${src%.$EXT}.expected"
    [ -f "$expected" ] || continue
    actual="$("$RENE" "$src" 2>&1 | tr -d '\r')"
    if [ "$actual" == "$(tr -d '\r' < "$expected")" ]; then
        echo "PASS  $(basename "$src")"
        pass=$((pass + 1))
    else
        echo "FAIL  $(basename "$src")"
        diff <(echo "$actual") <(tr -d '\r' < "$expected")
        fail=$((fail + 1))
    fi
done

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
