#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RESULTS_ROOT="$ROOT_DIR/results/key_recovery"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)_$$"
OUT_DIR=""
RESUME_MODE=0
PASS_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --resume-dir)
            if [ $# -lt 2 ]; then
                echo "Ошибка: для --resume-dir нужен путь"
                exit 1
            fi
            OUT_DIR="$2"
            RESUME_MODE=1
            shift 2
            ;;
        *)
            PASS_ARGS+=("$1")
            shift
            ;;
    esac
done

if [ -z "$OUT_DIR" ]; then
    OUT_DIR="$RESULTS_ROOT/$TIMESTAMP"
fi

mkdir -p "$BUILD_DIR" "$OUT_DIR"

echo "════════════════════════════════════════════════════════════════"
echo "  Эксперимент по восстановлению последних двух раундовых ключей"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Результаты будут сохранены в:"
echo "  $OUT_DIR"
echo ""

gcc -O3 -Wall -Wextra -std=c11 -march=native \
    -pthread \
    -o "$BUILD_DIR/key_recovery_experiment" \
    "$ROOT_DIR/tools/key_recovery_experiment.c" \
    -lm

echo "✓ Собран инструмент: $BUILD_DIR/key_recovery_experiment"
echo ""

if [ "$RESUME_MODE" -eq 1 ]; then
    "$BUILD_DIR/key_recovery_experiment" --output-dir "$OUT_DIR" --resume "${PASS_ARGS[@]}"
else
    "$BUILD_DIR/key_recovery_experiment" --output-dir "$OUT_DIR" "${PASS_ARGS[@]}"
fi

echo ""
echo "Готово."
echo "Сводка: $OUT_DIR/summary.csv"
