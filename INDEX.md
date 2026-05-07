# Навигация по документации

## Для преподавателя

### 🎯 Главное
1. **`ADVISOR_RESPONSE.md`** ← НАЧНИТЕ ЗДЕСЬ
   - Краткий ответ на комментарий преподавателя
   - Все результаты в одном файле
   - Команды для запуска

### 📊 Детали
2. **`QUICK_SUMMARY.md`**
   - Краткая сводка выполненной работы
   - Основные результаты
   - Оценки времени

3. **`SUMMARY_FOR_ADVISOR.md`**
   - Подробный отчёт с техническими деталями
   - Структура файлов
   - Использованные технологии

---

## Для работы с проектом

### Быстрый старт
- **`README.md`** — общее описание проекта МиниГОСТ
- **`docs/delta_pi_cheatsheet.md`** — быстрая справка по командам δπ
- **`docs/lookup_tables_cheatsheet.md`** — быстрая справка по lookup-таблицам

### Полная документация
- **`docs/delta_pi_guide.md`** — подробное руководство по вычислению δπ
- **`docs/lookup_tables_guide.md`** — подробное руководство по lookup-таблицам
- **`docs/algorithm.md`** — описание алгоритма МиниГОСТ
- **`docs/recommended_key.md`** — рекомендованный ключ и соглашения

---

## Результаты вычислений

### Текущий ключ (0xDEADBEEFCAFEBABE)
- **`results/delta_pi_result.txt`** — результат вычисления δπ
- **`verify_in_sage.py`** — скрипт для проверки в SageMath

### Batch-анализ (множество ключей)
- **`results/batch_analysis/`** — CSV файлы и статистика

### Lookup-таблицы
- **`lookup_tables/DEADBEEFCAFEBABE_lookup.h`** — C header
- **`lookup_tables/DEADBEEFCAFEBABE_lookup.bin`** — binary
- **`lookup_tables/DEADBEEFCAFEBABE_lookup.py`** — Python модуль
- **`lookup_tables/DEADBEEFCAFEBABE_lookup.md`** — human-readable

---

## Скрипты и программы

### Вычисление δπ
- **`run_delta_pi.sh`** — полное вычисление (~6 сек)
- **`batch_compute_delta_pi.sh`** — batch-анализ
- `tools/compute_delta_pi.c` — основная программа
- `tools/test_delta_pi.c` — быстрый тест
- `tools/compute_delta_pi_full.c` — с сохранением всех корреляций

### Lookup-таблицы
- **`generate_lookup.sh`** — генерация таблицы
- `tools/generate_lookup_table.py` — генератор
- `examples/example_lookup.c` — пример использования

### Проверка
- **`verify_in_sage.py`** — проверка в SageMath
- `tools/compare_minigost_impls.py` — сравнение реализаций
- `tools/prep_phase.py` — эталонные S-boxes

---

## Исторические документы

### Исправление ошибок
- **`BUGFIX.md`** — найденные и исправленные ошибки
- **`VERIFICATION.md`** — краткая проверка соответствия ТЗ
- **`SPECIFICATION_COMPLIANCE.md`** — подробное соответствие ТЗ

---

## Команды (краткая памятка)

### Быстрый тест δπ (~0.05 сек):
```bash
gcc -O3 -march=native -I. -o build/test_delta_pi tools/test_delta_pi.c -lm
./build/test_delta_pi
```

### Полное вычисление δπ (~6 сек):
```bash
./run_delta_pi.sh
```

### Batch-анализ (100 ключей, ~10 мин):
```bash
./batch_compute_delta_pi.sh 100
```

### Проверка в SageMath:
```bash
sage verify_in_sage.py
```

### Генерация lookup-таблицы:
```bash
./generate_lookup.sh
```

---

## Ключевые результаты

**Для ключа 0xDEADBEEFCAFEBABE:**
- δπ = 0.0240478516
- α = 0xCE65 (маска входа)
- β = 0x59A0 (маска выхода)
- Время: 5.84 секунд

**Производительность:**
- 1 ключ: ~6 сек
- 100 ключей: ~10 мин
- 1000 ключей: ~1.6 часа

---

**Дата:** 16 марта 2026
