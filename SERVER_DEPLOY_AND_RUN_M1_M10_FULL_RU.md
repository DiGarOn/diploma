# Полный сценарий деплоя и запуска `m1 / m10 / full_material`

Дата актуализации: 24 августа 2026 года.

Этот файл описывает полный практический сценарий: где арендовать сервер, как на него зайти, как перенести код, как проверить CUDA, как запустить прогон, как контролировать прогресс, как продолжить после сбоя и как забрать результаты.

## 1. Где арендовать сервер

Текущий рабочий вариант: [Vast.ai](https://vast.ai/).

Что выбирать:

- GPU: `RTX 4090`
- доступ: `SSH`
- ОС: обычный Linux-образ с CUDA или шаблон Vast с CUDA/SSH
- желательно:
  - не меньше `16` vCPU;
  - не меньше `60 GB` RAM;
  - не меньше `100 GB` свободного диска.

## 2. Что нужно от сервера после аренды

После создания инстанса нужны:

- `Public IP`
- `SSH port`

Подключение выглядит так:

```bash
ssh -p <SSH_PORT> root@<PUBLIC_IP>
```

Пример:

```bash
ssh -p 19549 root@175.155.64.164
```

## 3. Как перенести код на сервер

Рекомендуемый способ: через GitHub.

На сервере:

```bash
cd /workspace
git clone git@github.com:DiGarOn/diploma.git
cd diploma
git rev-parse --short HEAD
```

Если SSH-ключ к GitHub на сервер не добавлен, используйте HTTPS:

```bash
cd /workspace
git clone https://github.com/DiGarOn/diploma.git
cd diploma
git rev-parse --short HEAD
```

Если нужно быстро долить локальные незакоммиченные изменения, можно использовать `rsync` с ноутбука:

```bash
rsync -avh --progress \
  --exclude '.git' \
  --exclude 'build' \
  --exclude 'results' \
  --exclude 'from_server' \
  --exclude 'deliverables' \
  -e "ssh -p <SSH_PORT>" \
  /Users/dmitriydmitriygarkin/Documents/HSE/diploma/ \
  root@<PUBLIC_IP>:/workspace/diploma/
```

## 4. Что теперь считает suite

Текущий прогон состоит из трех серий:

1. `adaptive_m1`
2. `adaptive_m10`
3. `full_material`

Логика такая:

- в `adaptive_m1` вычисляется тяжелая prefix-часть;
- в `adaptive_m10` она переиспользуется через `--reuse-prefix-summary`;
- в `full_material` она тоже переиспользуется.

То есть тяжелый префиксный расчет не повторяется три раза.

## 5. Какие новые метрики добавлены

Для каждого эксперимента теперь считаются:

- `|delta_true|`
- среднее signed-значение по ложным ключам
- среднее `|delta_false|` по ложным ключам
- `|delta_true| / mean(|delta_false|)`
- `|delta_true| - mean(|delta_false|)`
- `max |delta_false|`
- число ложных ключей, на которых достигается `max |delta_false|`, с точностью до 7 знака после запятой

## 6. Проверка сервера перед большим запуском

После входа на сервер:

```bash
cd /workspace/diploma
nvidia-smi
nvcc --version
python3 --version
gcc --version
tmux -V
```

Запуск preflight:

```bash
bash tools/server_preflight.sh
```

Проверить результат:

```bash
cat results/server_preflight/build_backend.txt
tail -n 50 results/server_preflight/advisor_suite.log
```

Что должно быть:

- `build_backend=cuda`
- `nvidia-smi` работает
- `nvcc --version` работает
- `advisor_suite.log` не содержит падения smoke-run

## 7. Короткий тестовый запуск

Перед большим прогоном:

```bash
cd /workspace/diploma
bash tools/server_run_4090.sh --count 10 --threads 16
```

Проверка:

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/build_backend.txt"
cat "$RUN_DIR/suite_progress.txt"
cat "$RUN_DIR/adaptive_m1/run_config.txt"
cat "$RUN_DIR/adaptive_m1/progress.txt"
```

## 8. Полный запуск

Рекомендуется запускать через `tmux`, чтобы процесс пережил разрыв SSH.

Создать сессию:

```bash
tmux new -s diploma
```

Внутри `tmux`:

```bash
cd /workspace/diploma
bash tools/server_run_4090.sh --count 131072 --threads 16
```

Отсоединиться:

```bash
Ctrl+b d
```

Вернуться:

```bash
tmux attach -t diploma
```

## 9. Полный запуск в заранее заданную папку

Если хотите сразу зафиксировать имя каталога:

```bash
cd /workspace/diploma
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir /workspace/diploma/results/key_recovery_suite/m1_m10_full_131072_$(date +%Y%m%d_%H%M)
```

## 10. Как смотреть прогресс

Определить актуальный каталог:

```bash
RUN_DIR="$(ls -1dt /workspace/diploma/results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
```

Общий снимок:

```bash
cd /workspace/diploma
bash tools/server_tail_progress.sh "$RUN_DIR"
```

Ручная проверка:

```bash
cat "$RUN_DIR/suite_progress.txt"
cat "$RUN_DIR/adaptive_m1/progress.txt"
cat "$RUN_DIR/adaptive_m10/progress.txt"
cat "$RUN_DIR/full_material/progress.txt"
tail -n 50 "$RUN_DIR/suite_progress.log"
tail -n 50 "$RUN_DIR/adaptive_m1/progress.log"
```

Загрузка GPU:

```bash
watch -n 2 nvidia-smi
```

Если `watch` нет:

```bash
while true; do nvidia-smi; sleep 2; clear; done
```

## 11. Как продолжить после сбоя

Если сервер перезагрузился или процесс прервался, повторный запуск нужно делать в тот же каталог:

```bash
cd /workspace/diploma
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir /workspace/diploma/results/key_recovery_suite/<run_dir>
```

Скрипт поднимет `resume` и продолжит считать то, чего не хватает.

## 12. Какие файлы должны появиться после завершения

В корне каталога прогона:

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

По сериям:

- `adaptive_m1/summary.csv`
- `adaptive_m1/aggregate_report.csv`
- `adaptive_m1/aggregate_report.md`
- `adaptive_m1/run_config.txt`
- `adaptive_m1/progress.txt`
- `adaptive_m10/...`
- `full_material/...`

Также будет собрана преподавательская папка:

- `for_teacher_key_recovery_results_<run_dir>/`

## 13. Как забрать результаты с сервера

На ноутбуке:

```bash
rsync -avh --progress \
  --partial \
  --exclude='*.log' \
  --exclude='*.xlsx' \
  --exclude='*/top_candidates_*.csv' \
  -e "ssh -p <SSH_PORT> -o ServerAliveInterval=15 -o ServerAliveCountMax=6" \
  root@<PUBLIC_IP>:/workspace/diploma/results/key_recovery_suite/<run_dir>/ \
  /Users/dmitriydmitriygarkin/Documents/HSE/diploma/from_server/
```

Если нужен и teacher package, и сырой run, обычно достаточно скачать только сырой run, а teacher package пересобрать локально.

## 14. Как локально пересобрать teacher package

После скачивания:

```bash
cd /Users/dmitriydmitriygarkin/Documents/HSE/diploma
python3 tools/build_key_recovery_suite_artifacts.py \
  --suite-dir /Users/dmitriydmitriygarkin/Documents/HSE/diploma/from_server/<run_dir>
```

## 15. Что прислать при проблеме

Если что-то упало, достаточно этих команд:

```bash
RUN_DIR="$(ls -1dt /workspace/diploma/results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/build_backend.txt"
cat "$RUN_DIR/suite_progress.txt"
tail -n 100 "$RUN_DIR/suite_progress.log"
tail -n 100 "$RUN_DIR/adaptive_m1/progress.log"
tail -n 100 /workspace/diploma/results/server_preflight/advisor_suite.log
```

Если проблема на этапе сборки:

```bash
cd /workspace/diploma
bash tools/build_key_recovery_experiment.sh "$PWD" "$PWD/build"
```
