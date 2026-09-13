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
M1=1
M5=5
M10=10
FULL_M=100
BACKEND=auto
CUDA_THRESHOLD_COUNT=128
ONLY_SERIES=""

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
        --m1)
            M1="$2"
            shift 2
            ;;
        --m10)
            M10="$2"
            shift 2
            ;;
        --m5)
            M5="$2"
            shift 2
            ;;
        --full-m)
            FULL_M="$2"
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
        --only-series)
            ONLY_SERIES="$2"
            case "$ONLY_SERIES" in
                adaptive_m1|adaptive_m5|adaptive_m10|full_material)
                    ;;
                *)
                    echo "Неизвестная серия для --only-series: $ONLY_SERIES"
                    exit 1
                    ;;
            esac
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
if [ -n "$ONLY_SERIES" ]; then
    echo "Only series: $ONLY_SERIES"
fi
echo ""

bash "$ROOT_DIR/tools/build_key_recovery_experiment.sh" "$ROOT_DIR" "$BUILD_DIR"

python3 "$ROOT_DIR/tools/verify_th1h2t.py" \
    --key 0xDEADBEEF \
    --output "$SUITE_DIR/th1h2t_verification.csv"

run_series() {
    local label="$1"
    shift
    local out_dir="$SUITE_DIR/$label"
    local series_seed="$SEED"
    case "$label" in
        adaptive_m1)
            series_seed=$((SEED + 4000037))
            ;;
        adaptive_m5)
            series_seed=$((SEED + 5000011))
            ;;
        adaptive_m10)
            series_seed=$((SEED + 6000011))
            ;;
        full_material)
            series_seed=$((SEED + 7000003))
            ;;
    esac
    mkdir -p "$out_dir"

    echo "---- running $label seed=$series_seed ----"
    "$BUILD_DIR/key_recovery_experiment" \
        --output-dir "$out_dir" \
        --series-label "$label" \
        --count "$COUNT" \
        --seed "$series_seed" \
        --top "$TOP" \
        --threads "$THREADS" \
        --sample-cap "$SAMPLE_CAP" \
        --backend "$BACKEND" \
        --cuda-threshold-count "$CUDA_THRESHOLD_COUNT" \
        "$@"

    python3 "$ROOT_DIR/tools/summarize_key_recovery_run.py" --run-dir "$out_dir"
}

should_run_series() {
    local label="$1"
    if [ -z "$ONLY_SERIES" ]; then
        return 0
    fi
    [ "$ONLY_SERIES" = "$label" ]
}

if should_run_series "adaptive_m1"; then
    run_series "adaptive_m1" --m "$M1"
fi
if should_run_series "adaptive_m5"; then
    run_series "adaptive_m5" --m "$M5"
fi
if should_run_series "adaptive_m10"; then
    run_series "adaptive_m10" --m "$M10"
fi
if should_run_series "full_material"; then
    run_series "full_material" --full-material --m "$FULL_M"
fi

python3 - <<PY
import csv
from pathlib import Path

suite_dir = Path(r"$SUITE_DIR")
aggregate_paths = [
    suite_dir / "adaptive_m1" / "aggregate_report.csv",
    suite_dir / "adaptive_m5" / "aggregate_report.csv",
    suite_dir / "adaptive_m10" / "aggregate_report.csv",
    suite_dir / "full_material" / "aggregate_report.csv",
]
rows = []
for path in aggregate_paths:
    if not path.exists():
        continue
    with path.open(newline="") as f:
        rows.extend(csv.DictReader(f))

if rows:
    out_path = suite_dir / "series_overview.csv"
    with out_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
PY

cat > "$SUITE_DIR/README.txt" <<EOF
count=$COUNT
seed=$SEED
adaptive_m1_seed=$((SEED + 4000037))
adaptive_m5_seed=$((SEED + 5000011))
adaptive_m10_seed=$((SEED + 6000011))
full_material_seed=$((SEED + 7000003))
top=$TOP
threads=$THREADS
sample_cap=$SAMPLE_CAP
backend=$BACKEND
cuda_threshold_count=$CUDA_THRESHOLD_COUNT
series=adaptive_m1,adaptive_m5,adaptive_m10,full_material
only_series=${ONLY_SERIES:-all}
verification=th1h2t_verification.csv
overview=series_overview.csv
EOF

echo ""
echo "Suite completed."
echo "Suite dir: $SUITE_DIR"
echo "Overview: $SUITE_DIR/series_overview.csv"
