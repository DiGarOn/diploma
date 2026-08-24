#!/bin/bash

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Использование: bash tools/server_tail_progress.sh results/key_recovery_suite/<run>"
    exit 1
fi

SUITE_DIR="$1"

if [ ! -d "$SUITE_DIR" ]; then
    echo "Каталог не найден: $SUITE_DIR"
    exit 1
fi

echo "Suite dir: $SUITE_DIR"
echo ""

if [ -f "$SUITE_DIR/suite_progress.txt" ]; then
    echo "== suite_progress.txt =="
    cat "$SUITE_DIR/suite_progress.txt"
    echo ""
fi

for series in adaptive_m1 adaptive_m10 full_material; do
    if [ -f "$SUITE_DIR/$series/progress.txt" ]; then
        echo "== $series/progress.txt =="
        cat "$SUITE_DIR/$series/progress.txt"
        echo ""
    fi
done

echo "== tail suite_progress.log =="
if [ -f "$SUITE_DIR/suite_progress.log" ]; then
    tail -n 20 "$SUITE_DIR/suite_progress.log"
else
    echo "suite_progress.log not found"
fi
