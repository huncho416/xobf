#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

COMPILERS=(clang++ g++)
OPTS=(-O0 -O2 -O3)
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fail=0

for cxx in "${COMPILERS[@]}"; do
    if ! command -v "$cxx" >/dev/null 2>&1; then
        echo "skip: $cxx not found"
        continue
    fi
    for opt in "${OPTS[@]}"; do
        tbin="$TMP/tests_${cxx}_${opt}"
        dbin="$TMP/demo_${cxx}_${opt}"
        echo "=== $cxx $opt ==="
        "$cxx" -std=c++17 "$opt" -Iinclude tests/tests.cpp -o "$tbin"
        "$cxx" -std=c++17 "$opt" -Iinclude examples/demo.cpp -o "$dbin"
        if ! "$tbin"; then
            echo "TESTS FAILED: $cxx $opt"
            fail=1
        fi
        if ! bash tests/leak_check.sh "$dbin"; then
            echo "LEAK CHECK FAILED: $cxx $opt"
            fail=1
        fi
        out="$("$dbin")"
        if ! grep -q "SUPER_SECRET_API_TOKEN_MARKER" <<<"$out"; then
            echo "DEMO OUTPUT WRONG: $cxx $opt"
            fail=1
        fi
        if ! grep -q "PREMIUM_FEATURE_UNLOCKED_MARKER" <<<"$out"; then
            echo "DEMO OUTPUT WRONG (bogus branch): $cxx $opt"
            fail=1
        fi
        if grep -q "THIS_SHOULD_NEVER_PRINT_MARKER" <<<"$out"; then
            echo "DEAD BRANCH EXECUTED: $cxx $opt"
            fail=1
        fi
    done
done

if [[ "$fail" -eq 0 ]]; then
    echo "VERIFY PASSED"
else
    echo "VERIFY FAILED"
fi
exit "$fail"
