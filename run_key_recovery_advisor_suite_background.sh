#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_ROOT="$ROOT_DIR/results/key_recovery_suite"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)_$$"
SUITE_DIR=""
PASS_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --resume-dir|--suite-dir)
            SUITE_DIR="$2"
            PASS_ARGS+=("$1" "$2")
            shift 2
            ;;
        *)
            PASS_ARGS+=("$1")
            shift
            ;;
    esac
done

if [ -z "$SUITE_DIR" ]; then
    SUITE_DIR="$RESULTS_ROOT/$TIMESTAMP"
    PASS_ARGS+=("--suite-dir" "$SUITE_DIR")
fi

mkdir -p "$SUITE_DIR"

LOG_PATH="$SUITE_DIR/background_run.log"
PID_PATH="$SUITE_DIR/background_run.pid"
CMD=("$ROOT_DIR/run_key_recovery_advisor_suite.sh" "${PASS_ARGS[@]}")

echo "Запускаю background suite:"
echo "  suite dir: $SUITE_DIR"
echo "  log: $LOG_PATH"
echo ""

if command -v caffeinate >/dev/null 2>&1; then
    nohup caffeinate -dimsu "${CMD[@]}" > "$LOG_PATH" 2>&1 &
else
    nohup "${CMD[@]}" > "$LOG_PATH" 2>&1 &
fi

PID=$!
echo "$PID" > "$PID_PATH"

echo "PID: $PID"
echo "PID file: $PID_PATH"
echo "Tail log: tail -f '$LOG_PATH'"
echo ""
echo "Важно:"
echo "1. Полное выключение ноутбука остановит вычисления."
echo "2. Для стабильного прогона держи ноутбук на питании."
echo "3. Лучший локальный вариант: не закрывать крышку, выключить экран или заблокировать сеанс."
echo "4. Если закрывать крышку, на обычном MacBook вычисления обычно уйдут в сон, даже если процесс запущен через nohup/caffeinate."
