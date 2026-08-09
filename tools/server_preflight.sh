#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RESULTS_DIR="$ROOT_DIR/results/server_preflight"

run_with_time() {
    if [ -x /usr/bin/time ]; then
        /usr/bin/time -p "$@"
    else
        time "$@"
    fi
}

mkdir -p "$RESULTS_DIR"

echo "== System =="
uname -a || true
echo ""

echo "== CUDA tools =="
if command -v nvidia-smi >/dev/null 2>&1; then
    nvidia-smi || true
else
    echo "nvidia-smi: not found"
fi
echo ""

if command -v nvcc >/dev/null 2>&1; then
    nvcc --version || true
else
    echo "nvcc: not found"
fi
echo ""

echo "== Build =="
bash "$ROOT_DIR/tools/build_key_recovery_experiment.sh" "$ROOT_DIR" "$BUILD_DIR" | tee "$RESULTS_DIR/build_backend.txt"
BUILD_BACKEND="$(tail -n 1 "$RESULTS_DIR/build_backend.txt" | cut -d= -f2)"
echo "build_backend=$BUILD_BACKEND"
echo ""

echo "== CPU smoke =="
CPU_DIR="$RESULTS_DIR/cpu_smoke"
rm -rf "$CPU_DIR"
mkdir -p "$CPU_DIR"
run_with_time "$BUILD_DIR/key_recovery_experiment" \
    --output-dir "$CPU_DIR" \
    --count 1 \
    --top 0 \
    --threads 2 \
    --backend cpu \
    --series-label preflight_cpu \
    --m 10 | tee "$CPU_DIR/run.log"
echo ""

if [ "$BUILD_BACKEND" = "cuda" ]; then
    echo "== CUDA smoke =="
    CUDA_DIR="$RESULTS_DIR/cuda_smoke"
    rm -rf "$CUDA_DIR"
    mkdir -p "$CUDA_DIR"
    run_with_time "$BUILD_DIR/key_recovery_experiment" \
        --output-dir "$CUDA_DIR" \
        --count 1 \
        --top 0 \
        --threads 2 \
        --backend cuda \
        --series-label preflight_cuda \
        --m 10 | tee "$CUDA_DIR/run.log"
    echo ""
fi

echo "== Advisor suite smoke =="
SUITE_DIR="$RESULTS_DIR/advisor_suite_smoke"
rm -rf "$SUITE_DIR"
run_with_time "$ROOT_DIR/run_key_recovery_advisor_suite.sh" \
    --count 1 \
    --top 0 \
    --threads 2 \
    --backend auto \
    --cuda-threshold-count 1 \
    --resume-dir "$SUITE_DIR" | tee "$RESULTS_DIR/advisor_suite.log"
echo ""

echo "Preflight completed."
echo "Results: $RESULTS_DIR"
