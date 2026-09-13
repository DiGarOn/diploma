# Параллельный запуск key recovery suite на RTX 4090

## Идея

Чтобы не ждать весь suite последовательно на одном сервере, запускаем 4 серии на 4 отдельных RTX 4090:

```text
server 1: adaptive_m1
server 2: adaptive_m5
server 3: adaptive_m10
server 4: full_material
```

Все серверы используют один базовый `SEED`, но внутри запускателя серии получают разные фактические seed:

```text
adaptive_m1      seed = SEED + 4000037
adaptive_m5      seed = SEED + 5000011
adaptive_m10     seed = SEED + 6000011
full_material    seed = SEED + 7000003
```

## Команда на каждом сервере

После подключения к серверу:

```bash
cd /workspace/diploma
```

Сервер 1:

```bash
tmux new -s diploma_m1
bash tools/server_run_4090.sh --count 524288 --seed 2 --threads 16 --only-series adaptive_m1
```

Сервер 2:

```bash
tmux new -s diploma_m5
bash tools/server_run_4090.sh --count 524288 --seed 2 --threads 16 --only-series adaptive_m5
```

Сервер 3:

```bash
tmux new -s diploma_m10
bash tools/server_run_4090.sh --count 524288 --seed 2 --threads 16 --only-series adaptive_m10
```

Сервер 4:

```bash
tmux new -s diploma_full
bash tools/server_run_4090.sh --count 524288 --seed 2 --threads 16 --only-series full_material
```

Если терминал уже находится внутри tmux, новую nested-сессию создавать не нужно. Можно запускать команду сразу.

## Проверка статуса на сервере

Внутри `/workspace/diploma`:

```bash
tail -f results/key_recovery_suite/*/suite_progress.log
```

Или для конкретного каталога:

```bash
cat results/key_recovery_suite/RUN_DIR/suite_progress.txt
cat results/key_recovery_suite/RUN_DIR/suite_status.txt
cat results/key_recovery_suite/RUN_DIR/adaptive_m1/progress.txt
```

Заменить `adaptive_m1` на свою серию.

## Скачивание компактных результатов

Для каждой машины скачать только нужные компактные файлы. Пример:

```bash
rsync -avh --progress --partial --stats --prune-empty-dirs \
  -e "ssh -p SSH_PORT" \
  --include='*/' \
  --include='report.md' \
  --include='report.pdf' \
  --include='suite_config.txt' \
  --include='suite_status.txt' \
  --include='suite_progress.txt' \
  --include='series_overview.csv' \
  --include='series_overview_readable.csv' \
  --include='false_delta_distribution.csv' \
  --include='th1h2t_verification.csv' \
  --include='aggregate_report.csv' \
  --include='aggregate_report.md' \
  --include='run_config.txt' \
  --include='progress.txt' \
  --exclude='*' \
  root@SERVER_IP:/workspace/diploma/results/key_recovery_suite/RUN_DIR/ \
  /Users/dmitriydmitriygarkin/Documents/HSE/diploma/from_server_parallel/SERIES_NAME/
```

Если потребуется полный `summary.csv` для локальной склейки и расширенного анализа, добавить:

```bash
  --include='summary.csv' \
```

## Склейка результатов локально

После скачивания 4 папок:

```bash
cd /Users/dmitriydmitriygarkin/Documents/HSE/diploma
python3 tools/merge_key_recovery_partial_suites.py \
  --output-suite-dir from_server_parallel/merged_YYYYMMDD \
  from_server_parallel/adaptive_m1 \
  from_server_parallel/adaptive_m5 \
  from_server_parallel/adaptive_m10 \
  from_server_parallel/full_material
```

Скрипт скопирует найденные серии в общий suite и пересоберет:

```text
series_overview.csv
series_overview_readable.csv
false_delta_distribution.csv
report.md
report.pdf
for_teacher_key_recovery_results_*
```

## Оценка времени

Один RTX 4090 последовательно:

```text
adaptive_m1      ~32 часа
adaptive_m5      ~33 часа
adaptive_m10     ~34 часа
full_material    ~45 часов
итого            ~144 часа
```

4 RTX 4090 параллельно:

```text
wall-clock ~= самая длинная серия ~= 45 часов
```

После добавления хвостовой lookup-метрики ожидаемый overhead:

```text
lookup + новая метрика: около 0.015 сек/ключ после разовой инициализации LAT-таблиц
```

Итого ориентир:

```text
adaptive_m1      ~34.5 часа
adaptive_m5      ~35.0 часов
adaptive_m10     ~36.0 часов
full_material    ~47.7 часов

4 RTX 4090 параллельно: ~48 часов wall-clock, то есть около 2 суток
последовательно: ~153 GPU-часа
```

## Оценка бюджета

При цене около `$0.375/hr`:

```text
теоретическая стоимость по сумме GPU-часов: ~57 USD
практический резерв с учетом простоя/скачивания/разброса цен: 75-85 USD
```
