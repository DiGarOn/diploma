#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RESULTS_ROOT="$ROOT_DIR/results/key_recovery_suite"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)_$$"
REFERENCE_OVERVIEW="$ROOT_DIR/results/key_recovery_suite/20260523_124949_54914/series_overview.csv"

COUNT=131072
SEED=1
TOP=32
THREADS=10
SAMPLE_CAP=65536
M10=10
M100=100
FULL_M=100
BACKEND=auto
CUDA_THRESHOLD_COUNT=128
SUITE_DIR=""
CURRENT_PHASE="initializing"
SUITE_SUCCESS=0
SUITE_PROGRESS_LOG=""

write_status() {
    local state="$1"
    cat > "$SUITE_DIR/suite_status.txt" <<EOF
state=$state
phase=$CURRENT_PHASE
updated_at=$(date '+%Y-%m-%d %H:%M:%S %Z')
count=$COUNT
seed=$SEED
top=$TOP
threads=$THREADS
sample_cap=$SAMPLE_CAP
m10=$M10
m100=$M100
full_m=$FULL_M
suite_dir=$SUITE_DIR
EOF
}

count_completed_iterations() {
    local summary_path="$1"
    if [ ! -f "$summary_path" ]; then
        echo "0"
        return
    fi
    local lines
    lines=$(wc -l < "$summary_path")
    if [ "$lines" -le 1 ]; then
        echo "0"
    else
        echo $((lines - 1))
    fi
}

log_suite_progress() {
    local message="$1"
    printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S %Z')" "$message" | tee -a "$SUITE_PROGRESS_LOG"
}

write_series_progress_stub() {
    local label="$1"
    local out_dir="$2"
    local completed
    completed=$(count_completed_iterations "$out_dir/summary.csv")
    cat > "$out_dir/progress.txt" <<EOF
updated_at=$(date '+%Y-%m-%d %H:%M:%S %Z')
series_label=$label
completed_iterations=$completed
total_iterations=$COUNT
progress_percent=$(awk "BEGIN { if ($COUNT == 0) print \"0.0000\"; else printf \"%.4f\", 100*$completed/$COUNT }")
status=running_or_waiting
EOF
}

write_suite_progress_snapshot() {
    local adaptive_m10_done adaptive_m100_done full_material_done total_done total_expected
    adaptive_m10_done=$(count_completed_iterations "$SUITE_DIR/adaptive_m10/summary.csv")
    adaptive_m100_done=$(count_completed_iterations "$SUITE_DIR/adaptive_m100/summary.csv")
    full_material_done=$(count_completed_iterations "$SUITE_DIR/full_material/summary.csv")
    total_done=$((adaptive_m10_done + adaptive_m100_done + full_material_done))
    total_expected=$((COUNT * 3))

    cat > "$SUITE_DIR/suite_progress.txt" <<EOF
updated_at=$(date '+%Y-%m-%d %H:%M:%S %Z')
phase=$CURRENT_PHASE
adaptive_m10_completed=$adaptive_m10_done/$COUNT
adaptive_m100_completed=$adaptive_m100_done/$COUNT
full_material_completed=$full_material_done/$COUNT
total_completed=$total_done/$total_expected
log_file=$SUITE_DIR/background_run.log
EOF
}

handle_exit() {
    local status="$1"
    if [ "$SUITE_SUCCESS" -eq 1 ]; then
        return
    fi
    if [ -n "$SUITE_DIR" ] && [ -d "$SUITE_DIR" ]; then
        write_status "failed"
    fi
}

handle_signal() {
    if [ -n "$SUITE_DIR" ] && [ -d "$SUITE_DIR" ]; then
        write_status "interrupted"
    fi
    exit 130
}

trap 'status=$?; handle_exit "$status"' EXIT
trap 'handle_signal' INT TERM

while [ $# -gt 0 ]; do
    case "$1" in
        --resume-dir|--suite-dir)
            SUITE_DIR="$2"
            shift 2
            ;;
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
        --m10)
            M10="$2"
            shift 2
            ;;
        --m100)
            M100="$2"
            shift 2
            ;;
        --full-m)
            FULL_M="$2"
            shift 2
            ;;
        --reference-overview)
            REFERENCE_OVERVIEW="$2"
            shift 2
            ;;
        *)
            echo "Неизвестный аргумент: $1"
            exit 1
            ;;
    esac
done

if [ -z "$SUITE_DIR" ]; then
    SUITE_DIR="$RESULTS_ROOT/$TIMESTAMP"
fi

mkdir -p "$BUILD_DIR" "$SUITE_DIR"
SUITE_PROGRESS_LOG="$SUITE_DIR/suite_progress.log"
write_status "started"
write_suite_progress_snapshot
log_suite_progress "suite started; count=$COUNT threads=$THREADS suite_dir=$SUITE_DIR"

cat > "$SUITE_DIR/suite_config.txt" <<EOF
count=$COUNT
seed=$SEED
top=$TOP
threads=$THREADS
sample_cap=$SAMPLE_CAP
m10=$M10
m100=$M100
full_m=$FULL_M
backend=$BACKEND
cuda_threshold_count=$CUDA_THRESHOLD_COUNT
reference_overview=$REFERENCE_OVERVIEW
EOF

echo "════════════════════════════════════════════════════════════════"
echo "  Advisor Suite: key recovery experiments"
echo "════════════════════════════════════════════════════════════════"
echo "Suite dir: $SUITE_DIR"
echo "Count: $COUNT"
echo "Threads: $THREADS"
echo ""

if [ -f "$REFERENCE_OVERVIEW" ]; then
    CURRENT_PHASE="estimating_runtime"
    write_suite_progress_snapshot
    log_suite_progress "estimating runtime from $REFERENCE_OVERVIEW"
    python3 "$ROOT_DIR/tools/estimate_key_recovery_suite_time.py" \
        --overview-csv "$REFERENCE_OVERVIEW" \
        --target-count "$COUNT" | tee "$SUITE_DIR/time_estimate.txt"
    echo ""
fi

CURRENT_PHASE="compiling"
write_suite_progress_snapshot
log_suite_progress "compiling key_recovery_experiment"
bash "$ROOT_DIR/tools/build_key_recovery_experiment.sh" "$ROOT_DIR" "$BUILD_DIR" | tee "$SUITE_DIR/build_backend.txt"

CURRENT_PHASE="verifying_th1h2t"
write_suite_progress_snapshot
log_suite_progress "verifying TH1H2T"
if [ ! -f "$SUITE_DIR/th1h2t_verification.csv" ]; then
    python3 "$ROOT_DIR/tools/verify_th1h2t.py" \
        --key 0xDEADBEEF \
        --output "$SUITE_DIR/th1h2t_verification.csv"
fi

postprocess_suite() {
    CURRENT_PHASE="building_artifacts"
    write_suite_progress_snapshot
    log_suite_progress "building suite artifacts"
    python3 "$ROOT_DIR/tools/build_key_recovery_suite_artifacts.py" --suite-dir "$SUITE_DIR"
    write_suite_progress_snapshot
}

run_series() {
    local label="$1"
    shift
    local out_dir="$SUITE_DIR/$label"
    local reuse_prefix_summary="${REUSE_PREFIX_SUMMARY:-}"
    mkdir -p "$out_dir"
    write_series_progress_stub "$label" "$out_dir"

    CURRENT_PHASE="running_$label"
    write_status "running"
    write_suite_progress_snapshot
    log_suite_progress "starting series $label"

    echo "---- running $label ----"
    if [ -f "$out_dir/summary.csv" ]; then
        if [ -n "$reuse_prefix_summary" ]; then
            "$BUILD_DIR/key_recovery_experiment" \
                --output-dir "$out_dir" \
                --resume \
                --series-label "$label" \
                --count "$COUNT" \
                --seed "$SEED" \
                --top "$TOP" \
                --threads "$THREADS" \
                --sample-cap "$SAMPLE_CAP" \
                --backend "$BACKEND" \
                --cuda-threshold-count "$CUDA_THRESHOLD_COUNT" \
                --reuse-prefix-summary "$reuse_prefix_summary" \
                "$@"
        else
            "$BUILD_DIR/key_recovery_experiment" \
                --output-dir "$out_dir" \
                --resume \
                --series-label "$label" \
                --count "$COUNT" \
                --seed "$SEED" \
                --top "$TOP" \
                --threads "$THREADS" \
                --sample-cap "$SAMPLE_CAP" \
                --backend "$BACKEND" \
                --cuda-threshold-count "$CUDA_THRESHOLD_COUNT" \
                "$@"
        fi
    else
        if [ -n "$reuse_prefix_summary" ]; then
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
                --reuse-prefix-summary "$reuse_prefix_summary" \
                "$@"
        else
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
        fi
    fi

    CURRENT_PHASE="summarizing_$label"
    write_suite_progress_snapshot
    log_suite_progress "summarizing series $label"
    python3 "$ROOT_DIR/tools/summarize_key_recovery_run.py" --run-dir "$out_dir"
    write_suite_progress_snapshot
    log_suite_progress "series $label finished"
}

run_series "adaptive_m100" --m "$M100"
REUSE_PREFIX_SUMMARY="$SUITE_DIR/adaptive_m100/summary.csv" run_series "adaptive_m10" --m "$M10"
REUSE_PREFIX_SUMMARY="$SUITE_DIR/adaptive_m100/summary.csv" run_series "full_material" --full-material --m "$FULL_M"

CURRENT_PHASE="finalizing"
write_suite_progress_snapshot
log_suite_progress "finalizing suite"
postprocess_suite

write_status "completed"
CURRENT_PHASE="completed"
write_suite_progress_snapshot
log_suite_progress "suite completed"
SUITE_SUCCESS=1

echo ""
echo "Suite completed."
echo "Suite dir: $SUITE_DIR"
echo "Report: $SUITE_DIR/report.md"
echo "Package: $SUITE_DIR/for_teacher_key_recovery_results_$(basename "$SUITE_DIR")"
