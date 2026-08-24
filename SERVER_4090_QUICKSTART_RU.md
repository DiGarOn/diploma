# Быстрый запуск на RTX 4090

Полный сценарий развертывания и запуска находится в файле [SERVER_DEPLOY_AND_RUN_M1_M10_FULL_RU.md](/Users/dmitriydmitriygarkin/Documents/HSE/diploma/SERVER_DEPLOY_AND_RUN_M1_M10_FULL_RU.md).

Ниже только короткая памятка.

## 1. Где арендовать

Используется [Vast.ai](https://vast.ai/). Для текущего прогона нужен Linux-сервер с `RTX 4090`, SSH-доступом и рабочей CUDA.

## 2. Базовые команды

Подключение:

```bash
ssh -p <SSH_PORT> root@<PUBLIC_IP>
```

Клонирование:

```bash
git clone git@github.com:DiGarOn/diploma.git
cd diploma
```

Проверка среды:

```bash
nvidia-smi
nvcc --version
bash tools/server_preflight.sh
```

Короткий smoke-run:

```bash
bash tools/server_run_4090.sh --count 10 --threads 16
```

Полный запуск:

```bash
tmux new -s diploma
cd /workspace/diploma
bash tools/server_run_4090.sh --count 131072 --threads 16
```

Отсоединение:

```bash
Ctrl+b d
```

Возврат:

```bash
tmux attach -t diploma
```

## 3. Что теперь считается

Серии:

- `adaptive_m1`
- `adaptive_m10`
- `full_material`

Тяжелый prefix-расчет делается в `adaptive_m1`, дальше `adaptive_m10` и `full_material` используют `--reuse-prefix-summary`.

## 4. Как смотреть прогресс

```bash
RUN_DIR="$(ls -1dt results/key_recovery_suite/* | head -n 1)"
echo "$RUN_DIR"
bash tools/server_tail_progress.sh "$RUN_DIR"
cat "$RUN_DIR/adaptive_m1/progress.txt"
tail -n 50 "$RUN_DIR/adaptive_m1/progress.log"
watch -n 2 nvidia-smi
```

## 5. Как продолжить после обрыва

```bash
bash tools/server_run_4090.sh \
  --count 131072 \
  --threads 16 \
  --suite-dir results/key_recovery_suite/<run_dir>
```
