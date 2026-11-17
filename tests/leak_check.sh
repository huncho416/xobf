#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-}"
if [[ -z "$BIN" || ! -f "$BIN" ]]; then
    echo "usage: leak_check.sh <binary>" >&2
    exit 2
fi

MARKERS=(
    "SUPER_SECRET_API_TOKEN_MARKER"
    "PREMIUM_FEATURE_UNLOCKED_MARKER"
    "THIS_SHOULD_NEVER_PRINT_MARKER"
)

status=0
for m in "${MARKERS[@]}"; do
    if strings -a "$BIN" | grep -q "$m"; then
        echo "LEAK: $m found in plaintext in $BIN"
        status=1
    else
        echo "OK: $m absent"
    fi
done

if [[ "$status" -eq 0 ]]; then
    echo "LEAK CHECK PASSED"
fi
exit "$status"
