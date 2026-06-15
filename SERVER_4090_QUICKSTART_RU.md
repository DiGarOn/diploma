# Быстрый запуск на RTX 4090

Ниже минимальный и практичный сценарий, когда вы уже получили Linux-сервер с NVIDIA RTX 4090.

## 1. Что должно быть на сервере

Нужно:

- NVIDIA driver
- `nvidia-smi`
- CUDA toolkit с `nvcc`
- `git`
- `python3`
- `gcc`
- `tmux`

Быстрая проверка:

```bash
nvidia-smi
nvcc --version
python3 --version
gcc --version
tmux -V
git --version
```

## 2. Как скачать код

Если будете работать по SSH-ключу:

```bash
git clone git@github.com:DiGarOn/diploma.git
cd diploma
git rev-parse --short HEAD
```

Если удобнее по HTTPS:

```bash
git clone https://github.com/DiGarOn/diploma.git
cd diploma
git rev-parse --short HEAD
```

Если потом нужно подтянуть обновления:

```bash
git pull
```

## 3. Первая проверка, что сервер годится

Запустите встроенный preflight:

```bash
bash tools/server_preflight.sh
```

Что должно получиться:

- `nvidia-smi` видит GPU
- `nvcc --version` отрабатывает
- в `results/server_preflight/build_backend.txt` есть строка `build_backend=cuda`
- проходит `CPU smoke`
- проходит `CUDA smoke`
- проходит `Advisor suite smoke`

Если хотите быстро посмотреть итог:

```bash
cat results/server_preflight/build_backend.txt
tail -n 50 results/server_preflight/advisor_suite.log
```

## 4. Как сделать короткий тестовый запуск

Сначала лучше запустить не полный прогон, а короткий:

```bash
bash tools/server_run_4090.sh --count 10 --threads 16
```

Это уже идет через `backend=cuda`, то есть именно в том режиме, который нужен для сервера с `RTX 4090`.

После запуска найдите свежую папку:

```bash
ls -1dt results/key_recovery_suite/* | head
```

И посмотрите, что внутри:

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/suite_progress.txt"
cat "$RUN_DIR/adaptive_m100/progress.txt"
```

## 5. Как запускать полный прогон

Рекомендуемый способ: через `tmux`, чтобы процесс пережил разрыв SSH.

Создать сессию:

```bash
tmux new -s diploma
```

Внутри `tmux` запустить:

```bash
bash tools/server_run_4090.sh --count 131072 --threads 16
```

Отсоединиться от `tmux`:

```bash
Ctrl+b d
```

Подключиться обратно позже:

```bash
tmux attach -t diploma
```

## 6. Как контролировать прогресс

Узнать свежий run:

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
```

Краткий снимок прогресса:

```bash
bash tools/server_tail_progress.sh "$RUN_DIR"
```

Ручная проверка:

```bash
cat "$RUN_DIR/suite_progress.txt"
cat "$RUN_DIR/adaptive_m100/progress.txt"
tail -n 50 "$RUN_DIR/suite_progress.log"
tail -n 50 "$RUN_DIR/adaptive_m100/progress.log"
```

Полезно смотреть именно `adaptive_m100`, потому что там считается тяжелая часть. Серии `adaptive_m10` и `full_material` используют повторное использование prefix summary и проходят заметно быстрее.

## 7. Как понять, что CUDA реально используется

Проверьте:

```bash
cat "$RUN_DIR/build_backend.txt"
cat "$RUN_DIR/adaptive_m100/run_config.txt"
```

Там должно быть:

- `build_backend=cuda`
- `backend_mode=cuda` или `requested=cuda`
- `active_backend=cuda`

И можно параллельно глянуть загрузку GPU:

```bash
watch -n 2 nvidia-smi
```

Если `watch` не установлен:

```bash
while true; do nvidia-smi; sleep 2; clear; done
```

## 8. Как продолжить после сбоя

Если сервер перезагрузился, SSH оборвался или процесс прервался, не нужно начинать с нуля.

Найдите каталог прогона:

```bash
RUN_DIR="results/key_recovery_suite/<имя_вашего_прогона>"
```

И перезапустите с ним:

```bash
bash tools/server_run_4090.sh --count 131072 --threads 16 --suite-dir "$RUN_DIR"
```

Скрипт продолжит работу через `--resume-dir`.

## 9. Что отправлять мне при ошибке

Если на сервере что-то не пошло, удобно сразу прислать:

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
cat "$RUN_DIR/build_backend.txt"
tail -n 100 "$RUN_DIR/suite_progress.log"
tail -n 100 "$RUN_DIR/adaptive_m100/progress.log"
tail -n 100 results/server_preflight/advisor_suite.log
```

Если ошибка на этапе сборки, еще это:

```bash
bash tools/build_key_recovery_experiment.sh "$PWD" "$PWD/build"
```

## 10. Практический порядок действий

На новом сервере делайте так:

1. `git clone ... && cd diploma`
2. `bash tools/server_preflight.sh`
3. `bash tools/server_run_4090.sh --count 10 --threads 16`
4. Проверить `suite_progress.txt` и `run_config.txt`
5. Запустить полный прогон через `tmux`

Если хотите, дальше я могу еще подготовить для вас отдельный блок команд именно под `Ubuntu 22.04/24.04`: что установить одной командой перед первым запуском. 
