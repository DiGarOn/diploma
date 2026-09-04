#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
COUNT=524288
SEED="${SEED:-2}"
THREADS="${THREADS:-16}"
TOP="${TOP:-32}"
SUITE_DIR=""

while [ $# -gt 0 ]; do
    case "$1" in
        --count)
            COUNT="$2"
            shift 2
            ;;
        --threads)
            THREADS="$2"
            shift 2
            ;;
        --seed)
            SEED="$2"
            shift 2
            ;;
        --top)
            TOP="$2"
            shift 2
            ;;
        --resume-dir|--suite-dir)
            SUITE_DIR="$2"
            shift 2
            ;;
        *)
            echo "Неизвестный аргумент: $1"
            echo "Использование: bash tools/server_run_4090.sh [--count N] [--seed N] [--threads N] [--top N] [--suite-dir DIR]"
            exit 1
            ;;
    esac
done

CMD=(
    "$ROOT_DIR/run_key_recovery_advisor_suite.sh"
    --count "$COUNT"
    --seed "$SEED"
    --threads "$THREADS"
    --top "$TOP"
    --backend cuda
    --cuda-threshold-count 1
)

if [ -n "$SUITE_DIR" ]; then
    CMD+=(--resume-dir "$SUITE_DIR")
fi

echo "Запуск advisor suite под RTX 4090"
echo "ROOT_DIR=$ROOT_DIR"
echo "COUNT=$COUNT"
echo "SEED=$SEED"
echo "THREADS=$THREADS"
echo "TOP=$TOP"
if [ -n "$SUITE_DIR" ]; then
    echo "SUITE_DIR=$SUITE_DIR"
fi
echo ""
printf 'Команда:'
for arg in "${CMD[@]}"; do
    printf ' %q' "$arg"
done
printf '\n\n'

"${CMD[@]}"
