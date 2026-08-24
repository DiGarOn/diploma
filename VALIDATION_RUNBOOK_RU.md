# Валидация серверного запуска и кода

Этот файл фиксирует, какой код относится к текущему эксперименту `m1 / m10 / full_material`, что именно запускается на сервере и как это проверить.

## 1. Основные файлы

- [tools/key_recovery_experiment.c](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_experiment.c) — основной исполняемый код эксперимента.
- [tools/key_recovery_cuda.cu](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/key_recovery_cuda.cu) — CUDA-часть ускоренного расчета.
- [tools/build_key_recovery_experiment.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/build_key_recovery_experiment.sh) — сборка бинарника.
- [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh) — основной orchestration-скрипт.
- [tools/server_run_4090.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/server_run_4090.sh) — готовый серверный запуск для `RTX 4090`.
- [tools/server_preflight.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/server_preflight.sh) — preflight перед большим прогоном.
- [tools/summarize_key_recovery_run.py](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/summarize_key_recovery_run.py) — агрегирование одной серии.
- [tools/build_key_recovery_suite_artifacts.py](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/tools/build_key_recovery_suite_artifacts.py) — сборка итоговых артефактов по всему suite.

## 2. Что именно делает текущий suite

Скрипт [run_key_recovery_advisor_suite.sh](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/run_key_recovery_advisor_suite.sh):

1. сохраняет `suite_config.txt`, `suite_status.txt`, `suite_progress.txt`;
2. собирает бинарник `key_recovery_experiment`;
3. один раз проверяет корректность `TH1H2T`;
4. запускает три серии:
   - `adaptive_m1`;
   - `adaptive_m10`;
   - `full_material`;
5. для `adaptive_m10` и `full_material` переиспользует `prefix summary` из `adaptive_m1`;
6. после завершения строит итоговые CSV/MD/PDF-артефакты.

## 3. Какие новые метрики теперь пишутся

В `summary.csv` на уровень одного эксперимента теперь попадают:

- `TRUE_DELTA_ABS_*`
- `FALSE_DELTA_ABS_MEAN_*`
- `FALSE_DELTA_ABS_MAX_*`
- `FALSE_DELTA_ABS_MAX_KEY_COUNT_TOL_1E_7`
- `TRUE_FALSE_ABS_RATIO_VALUE`
- `TRUE_FALSE_ABS_DIFF_*`

Поле `FALSE_DELTA_ABS_MAX_KEY_COUNT_TOL_1E_7` означает число ложных ключей среди `65535`, на которых `max |delta_false|` достигался в данном эксперименте, причем совпадение берется с точностью до 7 знака после запятой.

## 4. Что проверять на новом сервере

Минимальный сценарий:

```bash
git clone git@github.com:DiGarOn/diploma.git
cd diploma

bash tools/server_preflight.sh
bash tools/server_run_4090.sh --count 10 --threads 16
```

После короткого запуска:

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/build_backend.txt"
cat "$RUN_DIR/suite_config.txt"
cat "$RUN_DIR/adaptive_m1/run_config.txt"
bash tools/server_tail_progress.sh "$RUN_DIR"
```

## 5. Как проверить, что считалось именно на CUDA

```bash
cat results/server_preflight/build_backend.txt
cat results/key_recovery_suite/<run_dir>/build_backend.txt
cat results/key_recovery_suite/<run_dir>/adaptive_m1/run_config.txt
```

Ожидаемые признаки:

- `build_backend=cuda`
- `backend_mode=cuda`
- `active_backend=cuda`
- `cuda_available=yes`

## 6. Какие артефакты являются выходом

В каталоге `results/key_recovery_suite/<run_dir>/` должны быть:

- `suite_config.txt`
- `suite_status.txt`
- `suite_progress.txt`
- `suite_progress.log`
- `build_backend.txt`
- `th1h2t_verification.csv`
- `series_overview.csv`
- `series_overview_readable.csv`
- `false_delta_distribution.csv`
- `report.md`
- `report.pdf`
- по подкаталогу на каждую серию:
  - `summary.csv`
  - `aggregate_report.csv`
  - `aggregate_report.md`
  - `run_config.txt`
  - `progress.txt`

## 7. Что является служебным

Служебные или generated-данные:

- `from_server/`
- `deliverables/`
- `results/key_recovery/`
- `results/key_recovery_suite/`
- `results/server_preflight/`
- `build/`
- `__pycache__/`
