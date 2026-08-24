# Runbook for CUDA Server

For the current experiment setup, start from:

- [SERVER_DEPLOY_AND_RUN_M1_M10_FULL_RU.md](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/SERVER_DEPLOY_AND_RUN_M1_M10_FULL_RU.md)
- [SERVER_4090_QUICKSTART_RU.md](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/SERVER_4090_QUICKSTART_RU.md)

These files describe the exact workflow for the `m1 / m10 / full_material` run on a rented CUDA server.

## Current execution model

The suite now runs three series:

- `adaptive_m1`
- `adaptive_m10`
- `full_material`

The expensive prefix stage is computed once in `adaptive_m1`. The next two series reuse it through `--reuse-prefix-summary`.

## Recommended launch command

```bash
bash tools/server_run_4090.sh --count 131072 --threads 16
```

## Resume command

```bash
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir results/key_recovery_suite/<run_dir>
```

## Main files to inspect

- `results/key_recovery_suite/<run>/suite_progress.txt`
- `results/key_recovery_suite/<run>/suite_progress.log`
- `results/key_recovery_suite/<run>/adaptive_m1/progress.txt`
- `results/key_recovery_suite/<run>/adaptive_m1/progress.log`
- `results/key_recovery_suite/<run>/adaptive_m1/run_config.txt`
- `results/key_recovery_suite/<run>/build_backend.txt`

## Minimal diagnostics

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/build_backend.txt"
cat "$RUN_DIR/suite_progress.txt"
tail -n 50 "$RUN_DIR/suite_progress.log"
tail -n 50 "$RUN_DIR/adaptive_m1/progress.log"
cat "$RUN_DIR/adaptive_m1/run_config.txt"
```
