# Runbook for CUDA Server

If you are renting a machine specifically with `RTX 4090`, start from:

- [SERVER_4090_QUICKSTART_RU.md](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/SERVER_4090_QUICKSTART_RU.md)

That file contains the exact clone, preflight, launch, resume, and progress-check commands in Russian.

## 1. Best upload workflow

The safest workflow is:

1. Push code to GitHub from the laptop.
2. SSH into the rented server.
3. Clone the repository on the server.
4. Run a short preflight before the big job.

This is better than copying files by hand because:

- you always know exactly which commit is running;
- it is easier to diff and roll back changes;
- debugging is simpler because logs and code match one commit hash.

Recommended clone command:

```bash
git clone git@github.com:DiGarOn/diploma.git
cd diploma
```

If you need to move local uncommitted changes quickly, use `rsync`:

```bash
rsync -av --delete \
  --exclude '.git' \
  --exclude 'results' \
  /path/to/diploma/ user@server:/path/to/diploma/
```

Use `git` for normal work and `rsync` only for emergency syncs.

## 2. First commands on the server

Check the GPU and CUDA toolchain:

```bash
nvidia-smi
nvcc --version
```

Run the built-in preflight:

```bash
bash tools/server_preflight.sh
```

This script checks:

- `nvidia-smi`
- `nvcc`
- binary build
- CPU smoke test
- CUDA smoke test if CUDA build succeeds
- short advisor suite smoke test

## 3. Recommended long-run workflow

Use `tmux` so the job survives SSH disconnects:

```bash
tmux new -s diploma
```

Inside `tmux`, start the run:

```bash
bash tools/server_run_4090.sh --count 131072 --threads 16
```

Detach from `tmux` with:

```bash
Ctrl+b d
```

Return later:

```bash
tmux attach -t diploma
```

## 4. Where to look when something breaks

Main places:

- suite-level progress: `results/key_recovery_suite/<run>/suite_progress.txt`
- suite-level log: `results/key_recovery_suite/<run>/suite_progress.log`
- per-series progress: `results/key_recovery_suite/<run>/<series>/progress.txt`
- per-series log: `results/key_recovery_suite/<run>/<series>/progress.log`
- per-series config: `results/key_recovery_suite/<run>/<series>/run_config.txt`
- build result: `results/key_recovery_suite/<run>/build_backend.txt`

Useful commands:

```bash
tail -n 50 results/key_recovery_suite/<run>/suite_progress.log
tail -n 50 results/key_recovery_suite/<run>/adaptive_m100/progress.log
cat results/key_recovery_suite/<run>/adaptive_m100/run_config.txt
```

## 5. Typical failure cases

### `nvcc: not found`

CUDA toolkit is not installed or `PATH` is wrong.

### `backend=cuda` requested but CUDA unavailable

The binary was built without CUDA support or the driver/runtime is broken.

Check:

```bash
cat results/key_recovery_suite/<run>/build_backend.txt
nvidia-smi
nvcc --version
```

### CUDA build fails but CPU build works

Run:

```bash
bash tools/build_key_recovery_experiment.sh "$PWD" "$PWD/build"
```

If it prints `build_backend=cpu_stub`, the helper fell back to CPU-only mode.

### The long run stops midway

Resume with the same suite directory:

```bash
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir results/key_recovery_suite/<run>
```

## 6. Practical recommendation

Before the full run:

1. `git pull`
2. `bash tools/server_preflight.sh`
3. `./run_key_recovery_advisor_suite.sh --count 10 ...`
4. only then launch the full `131072`

That catches almost all environment problems early.
