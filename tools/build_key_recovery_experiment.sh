#!/bin/bash

set -euo pipefail

ROOT_DIR="${1:?usage: build_key_recovery_experiment.sh ROOT_DIR BUILD_DIR}"
BUILD_DIR="${2:?usage: build_key_recovery_experiment.sh ROOT_DIR BUILD_DIR}"

mkdir -p "$BUILD_DIR"

CPU_OBJ="$BUILD_DIR/key_recovery_experiment.o"
CUDA_OBJ="$BUILD_DIR/key_recovery_cuda.o"
BIN_PATH="$BUILD_DIR/key_recovery_experiment"

gcc -O3 -Wall -Wextra -std=c11 -march=native \
    -c "$ROOT_DIR/tools/key_recovery_experiment.c" \
    -o "$CPU_OBJ"

if command -v nvcc >/dev/null 2>&1; then
    if nvcc -O3 -std=c++17 \
        -c "$ROOT_DIR/tools/key_recovery_cuda.cu" \
        -o "$CUDA_OBJ"
    then
        nvcc -O3 \
            -o "$BIN_PATH" \
            "$CPU_OBJ" \
            "$CUDA_OBJ" \
            -Xcompiler -pthread \
            -lm
        echo "build_backend=cuda"
        exit 0
    fi
    echo "warning: nvcc found, but CUDA object build failed; falling back to CPU-only stub" >&2
fi

gcc -O3 -Wall -Wextra -std=c11 -march=native \
    -pthread \
    -o "$BIN_PATH" \
    "$CPU_OBJ" \
    "$ROOT_DIR/tools/key_recovery_cuda_stub.c" \
    -lm

echo "build_backend=cpu_stub"
