# Валидация серверного запуска и кода

Этот файл фиксирует, какой код относится к экспериментам по восстановлению ключа, что именно запускалось на сервере и как это можно перепроверить.

## 1. Какие файлы являются основными

- [tools/key_recovery_experiment.c](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_experiment.c) — основная реализация эксперимента. Здесь находятся:
  - выбор backend `cpu/auto/cuda`;
  - поддержка `--resume`;
  - запись `run_config.txt`;
  - запись `summary.csv`;
  - повторное использование ранее посчитанного prefix summary через `--reuse-prefix-summary`.
- [tools/key_recovery_cuda.cu](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_cuda.cu) — CUDA-реализация ускоренного расчета prefix spectrum.
- [tools/key_recovery_cuda_backend.h](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_cuda_backend.h) — интерфейс между C-частью и CUDA-частью.
- [tools/key_recovery_cuda_stub.c](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_cuda_stub.c) — CPU fallback, если CUDA недоступна.
- [tools/build_key_recovery_experiment.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/build_key_recovery_experiment.sh) — сборка бинарника с автоматическим выбором `cuda` или `cpu_stub`.
- [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh) — orchestration-скрипт полного набора серий.
- [tools/server_preflight.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/server_preflight.sh) — проверка сервера перед большим прогоном.
- [tools/server_run_4090.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/server_run_4090.sh) — готовый серверный запуск для RTX 4090.
- [tools/server_tail_progress.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/server_tail_progress.sh) — быстрый просмотр прогресса.
- [tools/build_key_recovery_suite_artifacts.py](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/build_key_recovery_suite_artifacts.py) — сборка итоговых таблиц и teacher package.

## 2. Что именно запускалось на сервере

Использовался Linux-сервер с NVIDIA RTX 4090. Большой прогон, результаты которого потом были выгружены, был запущен в каталоге `/workspace/diploma_run` 15 июня 2026 года.

Фактическая команда большого прогона:

```bash
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir /workspace/diploma_run/results/key_recovery_suite/full_131072_20260615_1905
```

Этот скрипт принудительно запускает suite в режиме:

```bash
./run_key_recovery_advisor_suite.sh \
  --count 131072 \
  --threads 16 \
  --top 32 \
  --backend cuda \
  --cuda-threshold-count 1 \
  --resume-dir /workspace/diploma_run/results/key_recovery_suite/full_131072_20260615_1905
```

То есть на сервере использовался именно CUDA-backend, а не `auto`.

## 3. Что делает основной suite

Скрипт [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh):

1. сохраняет `suite_config.txt`, `suite_status.txt`, `suite_progress.txt`;
2. оценивает время по референсному предыдущему прогону;
3. собирает бинарник `key_recovery_experiment`;
4. один раз проверяет корректность TH1H2T на истинном ключе;
5. считает три серии:
   - `adaptive_m100`;
   - `adaptive_m10`;
   - `full_material`;
6. для двух последних серий переиспользует уже посчитанный prefix summary из `adaptive_m100`, чтобы не делать повторный тяжелый расчет;
7. строит итоговые CSV/MD/PDF-артефакты.

## 4. Где в коде видна воспроизводимость

Ключевые места:

- [tools/key_recovery_experiment.c](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_experiment.c:549)
  Здесь выбирается активный backend:
  - если передан `--reuse-prefix-summary`, расчет prefix spectrum повторно не делается;
  - если запрошен `--backend cuda`, программа требует доступную CUDA;
  - в режиме `auto` CUDA выбирается только при выполнении порога `cuda_threshold_count`.

- [tools/key_recovery_experiment.c](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_experiment.c:1579)
  Здесь в `run_config.txt` записываются:
  - `backend_mode`;
  - `active_backend`;
  - `cuda_threshold_count`;
  - `cuda_available`;
  - `cuda_status`;
  - `reuse_prefix_summary`;
  - `completed_before_start`.

- [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh:185)
  Здесь сохраняется `suite_config.txt`.

- [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh:79)
  Здесь формируется общий `suite_progress.txt`.

- [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh:222)
  Здесь выполняется одноразовая проверка `TH1H2T`.

- [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh:253)
  Здесь реализован `resume` по уже существующему `summary.csv`.

- [tools/build_key_recovery_experiment.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/build_key_recovery_experiment.sh:18)
  Здесь видно, что при наличии `nvcc` и успешной сборке получается `build_backend=cuda`, иначе происходит fallback на `cpu_stub`.

- [tools/server_run_4090.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/server_run_4090.sh:37)
  Здесь видно, что запуск для RTX 4090 выставляет `--backend cuda` и `--cuda-threshold-count 1`.

## 5. Как воспроизвести проверку на новом сервере

Минимальный сценарий:

```bash
git clone git@github.com:DiGarOn/diploma.git
cd diploma

bash tools/server_preflight.sh

bash tools/server_run_4090.sh --count 10 --threads 16
```

После короткого smoke-запуска:

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/build_backend.txt"
cat "$RUN_DIR/suite_config.txt"
cat "$RUN_DIR/adaptive_m100/run_config.txt"
bash tools/server_tail_progress.sh "$RUN_DIR"
```

Для полного повтора большого прогона:

```bash
bash tools/server_run_4090.sh --count 131072 --threads 16
```

Если запуск оборвался:

```bash
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir results/key_recovery_suite/<run_dir>
```

## 6. Как проверить, что считалось именно на CUDA

Нужно посмотреть:

```bash
cat results/server_preflight/build_backend.txt
cat results/key_recovery_suite/<run_dir>/build_backend.txt
cat results/key_recovery_suite/<run_dir>/adaptive_m100/run_config.txt
```

Ожидаемые признаки:

- `build_backend=cuda`;
- `backend_mode=cuda`;
- `active_backend=cuda`;
- `cuda_available=yes`.

Дополнительно можно контролировать загрузку GPU:

```bash
nvidia-smi
```

## 7. Какие артефакты являются выходом большого прогона

В каталоге `results/key_recovery_suite/<run_dir>/` должны быть:

- `suite_config.txt`;
- `suite_status.txt`;
- `suite_progress.txt`;
- `suite_progress.log`;
- `build_backend.txt`;
- `th1h2t_verification.csv`;
- `series_overview.csv`;
- `report.md`;
- по подкаталогу на каждую серию:
  - `summary.csv`;
  - `aggregate_report.csv`;
  - `run_config.txt`;
  - `progress.txt`.

## 8. Что в репозитории является служебным и не относится к ядру кода

Служебные или generated-данные:

- `from_server/`;
- `deliverables/`;
- `results/key_recovery/`;
- `results/key_recovery_suite/`;
- `results/server_preflight/`;
- `build/`;
- `__pycache__/`.

Они не нужны для чтения исходного кода и не являются исходниками алгоритма.
