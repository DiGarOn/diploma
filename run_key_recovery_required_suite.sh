#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RESULTS_ROOT="$ROOT_DIR/results/key_recovery_suite"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)_$$"
SUITE_DIR="$RESULTS_ROOT/$TIMESTAMP"

COUNT=1000
SEED=1
TOP=32
THREADS=10
SAMPLE_CAP=65536
BACKEND=auto
CUDA_THRESHOLD_COUNT=128

while [ $# -gt 0 ]; do
    case "$1" in
        --count)
            COUNT="$2"
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
        --threads)
            THREADS="$2"
            shift 2
            ;;
        --sample-cap)
            SAMPLE_CAP="$2"
            shift 2
            ;;
        --backend)
            BACKEND="$2"
            shift 2
            ;;
        --cuda-threshold-count)
            CUDA_THRESHOLD_COUNT="$2"
            shift 2
            ;;
        *)
            echo "Неизвестный аргумент: $1"
            exit 1
            ;;
    esac
done

mkdir -p "$BUILD_DIR" "$SUITE_DIR"

echo "════════════════════════════════════════════════════════════════"
echo "  Suite: key recovery experiments"
echo "════════════════════════════════════════════════════════════════"
echo "Suite dir: $SUITE_DIR"
echo ""

bash "$ROOT_DIR/tools/build_key_recovery_experiment.sh" "$ROOT_DIR" "$BUILD_DIR"

python3 "$ROOT_DIR/tools/verify_th1h2t.py" \
    --key 0xDEADBEEF \
    --output "$SUITE_DIR/th1h2t_verification.csv"

run_series() {
    local label="$1"
    shift
    local out_dir="$SUITE_DIR/$label"
    mkdir -p "$out_dir"

    echo "---- running $label ----"
    "$BUILD_DIR/key_recovery_experiment" \
        --output-dir "$out_dir" \
        --series-label "$label" \
        --count "$COUNT" \
        --seed "$SEED" \
        --top "$TOP" \
        --threads "$THREADS" \
        --sample-cap "$SAMPLE_CAP" \
        --backend "$BACKEND" \
        --cuda-threshold-count "$CUDA_THRESHOLD_COUNT" \
        "$@"

    python3 "$ROOT_DIR/tools/summarize_key_recovery_run.py" --run-dir "$out_dir"
}

run_series "adaptive_m10" --m 10
run_series "adaptive_m100" --m 100
run_series "full_material" --full-material --m 100

python3 - <<PY
import csv
from pathlib import Path

suite_dir = Path(r"$SUITE_DIR")
aggregate_paths = [
    suite_dir / "adaptive_m10" / "aggregate_report.csv",
    suite_dir / "adaptive_m100" / "aggregate_report.csv",
    suite_dir / "full_material" / "aggregate_report.csv",
]
rows = []
for path in aggregate_paths:
    with path.open(newline="") as f:
        rows.extend(csv.DictReader(f))

out_path = suite_dir / "series_overview.csv"
with out_path.open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)
PY

cat > "$SUITE_DIR/README.txt" <<EOF
count=$COUNT
seed=$SEED
top=$TOP
threads=$THREADS
sample_cap=$SAMPLE_CAP
backend=$BACKEND
cuda_threshold_count=$CUDA_THRESHOLD_COUNT
series=adaptive_m10,adaptive_m100,full_material
verification=th1h2t_verification.csv
overview=series_overview.csv
EOF

echo ""
echo "Suite completed."
echo "Suite dir: $SUITE_DIR"
echo "Overview: $SUITE_DIR/series_overview.csv"
