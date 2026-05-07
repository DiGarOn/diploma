#!/bin/bash
# Batch вычисление δπ для множества ключей
# Использование: ./batch_compute_delta_pi.sh [количество_ключей]

set -e

NUM_KEYS=${1:-10}  # По умолчанию 10 ключей
RESULTS_DIR="results/batch_analysis"
TEMP_DIR="build/temp_lookup"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║      Batch-анализ линейной характеристики МиниГОСТ           ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Параметры:"
echo "  Количество ключей: $NUM_KEYS"
echo "  Директория результатов: $RESULTS_DIR"
echo ""

# Создаём директории
mkdir -p "$RESULTS_DIR"
mkdir -p "$TEMP_DIR"
mkdir -p build

# Проверяем наличие основной программы
if [ ! -f "tools/compute_delta_pi_batch.c" ]; then
    echo "Создаю программу для batch-вычислений..."
    
    # Создаём упрощённую версию без lookup-таблицы в коде
    cat > tools/compute_delta_pi_batch.c << 'EOFPROG'
/**
 * Batch вычисление δπ с lookup-таблицей из файла
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>

static inline void fwt_int_65536(int *a) {
    for (int step = 1; step < 65536; step <<= 1) {
        for (int i = 0; i < 65536; i += 2 * step) {
            for (int j = 0; j < step; j++) {
                int u = a[i + j];
                int v = a[i + j + step];
                a[i + j] = u + v;
                a[i + j + step] = u - v;
            }
        }
    }
}

static inline int parity16(uint16_t x) {
    return __builtin_parity(x);
}

typedef struct {
    float delta_pi;
    uint16_t alpha;
    uint16_t beta;
    float correlation;
    double time_sec;
} Result;

Result compute_delta_pi_for_table(const uint16_t *lookup_table) {
    Result result = {0};
    const float norm = 1.0f / 65536.0f;
    
    int *f = (int*)malloc(65536 * sizeof(int));
    if (!f) {
        result.delta_pi = -1.0f;
        return result;
    }
    
    clock_t start = clock();
    
    for (int beta = 1; beta < 65536; beta++) {
        for (int x = 0; x < 65536; x++) {
            uint16_t y = lookup_table[x];
            f[x] = parity16(beta & y) ? -1 : 1;
        }
        
        fwt_int_65536(f);
        
        for (int alpha = 1; alpha < 65536; alpha++) {
            float corr = -(float)f[alpha] * norm;
            float abs_corr = fabsf(corr);
            
            if (abs_corr > result.delta_pi) {
                result.delta_pi = abs_corr;
                result.alpha = alpha;
                result.beta = beta;
                result.correlation = corr;
            }
        }
    }
    
    clock_t end = clock();
    result.time_sec = (double)(end - start) / CLOCKS_PER_SEC;
    
    free(f);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <lookup_table.bin> <key_hex>\n", argv[0]);
        return 1;
    }
    
    const char *table_file = argv[1];
    const char *key_str = argv[2];
    uint32_t key = (uint32_t)strtoul(key_str, NULL, 16);
    
    // Загружаем таблицу
    FILE *f = fopen(table_file, "rb");
    if (!f) {
        fprintf(stderr, "Ошибка открытия %s\n", table_file);
        return 1;
    }
    
    uint16_t *lookup_table = (uint16_t*)malloc(65536 * sizeof(uint16_t));
    if (!lookup_table) {
        fprintf(stderr, "Ошибка выделения памяти\n");
        fclose(f);
        return 1;
    }
    
    size_t read = fread(lookup_table, sizeof(uint16_t), 65536, f);
    fclose(f);
    
    if (read != 65536) {
        fprintf(stderr, "Ошибка чтения таблицы: прочитано %zu элементов\n", read);
        free(lookup_table);
        return 1;
    }
    
    // Вычисляем δπ
    Result result = compute_delta_pi_for_table(lookup_table);
    free(lookup_table);
    
    if (result.delta_pi < 0) {
        fprintf(stderr, "Ошибка вычисления\n");
        return 1;
    }
    
    // Выводим результат в CSV формате
    printf("0x%08X,%.10f,0x%04X,0x%04X,%.10f,%.2f\n",
           key, result.delta_pi, result.alpha, result.beta, 
           result.correlation, result.time_sec);
    
    return 0;
}
EOFPROG
fi

# Компилируем программу
echo "Компиляция программы..."
gcc -O3 -march=native -o build/compute_delta_pi_batch tools/compute_delta_pi_batch.c -lm
echo "✓ Готово"
echo ""

# Создаём файл результатов
RESULT_FILE="$RESULTS_DIR/delta_pi_results_$(date +%Y%m%d_%H%M%S).csv"
echo "KEY,DELTA_PI,ALPHA,BETA,CORRELATION,MAX_COUNT,TIME_SEC" > "$RESULT_FILE"

echo "Начинаю вычисления..."
echo ""

# Генерируем ключи и вычисляем δπ
START_TIME=$(date +%s)

for i in $(seq 1 $NUM_KEYS); do
    # Генерируем случайный ключ
    KEY=$(openssl rand -hex 4 | tr '[:lower:]' '[:upper:]')
    
    echo "[$i/$NUM_KEYS] Ключ: 0x$KEY"
    
    # Генерируем lookup-таблицу
    LOOKUP_FILE="$TEMP_DIR/${KEY}_lookup.bin"
    
    python3 - << EOFPY "$KEY" "$LOOKUP_FILE"
import sys
import struct
sys.path.insert(0, '.')
from minigost import MiniGost

key = int(sys.argv[1], 16)
output_file = sys.argv[2]

cipher = MiniGost()
lookup = [cipher.encrypt_block(x, key) for x in range(65536)]

with open(output_file, 'wb') as f:
    f.write(struct.pack('<65536H', *lookup))
EOFPY
    
    # Вычисляем δπ
    ./build/compute_delta_pi_batch "$LOOKUP_FILE" "$KEY" >> "$RESULT_FILE"
    
    # Удаляем временную таблицу
    rm -f "$LOOKUP_FILE"
    
    echo "  δπ вычислен"
    echo ""
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                      ГОТОВО                                   ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Результаты сохранены в: $RESULT_FILE"
echo "Всего ключей: $NUM_KEYS"
echo "Общее время: ${ELAPSED} секунд ($(echo "scale=2; $ELAPSED/60" | bc) минут)"
echo "Среднее время на ключ: $(echo "scale=2; $ELAPSED/$NUM_KEYS" | bc) секунд"
echo ""

# Создаём скрипт для анализа результатов
ANALYSIS_SCRIPT="$RESULTS_DIR/analyze_results.py"

cat > "$ANALYSIS_SCRIPT" << 'EOFANALYSIS'
#!/usr/bin/env python3
"""
Анализ batch-результатов вычисления δπ
"""

import sys
import csv
import statistics
from pathlib import Path

if len(sys.argv) < 2:
    print("Использование: python3 analyze_results.py <результаты.csv>")
    sys.exit(1)

csv_file = sys.argv[1]

# Читаем данные
delta_pis = []
times = []
keys = []

with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        delta_pis.append(float(row['DELTA_PI']))
        times.append(float(row['TIME_SEC']))
        keys.append(row['KEY'])

print("═" * 70)
print(" " * 20 + "СТАТИСТИЧЕСКИЙ АНАЛИЗ")
print("═" * 70)
print()

print(f"Количество ключей: {len(delta_pis)}")
print()

print("Линейная характеристика δπ:")
print(f"  Среднее:      {statistics.mean(delta_pis):.10f}")
print(f"  Медиана:      {statistics.median(delta_pis):.10f}")
print(f"  Минимум:      {min(delta_pis):.10f}")
print(f"  Максимум:     {max(delta_pis):.10f}")
if len(delta_pis) > 1:
    print(f"  Ст. откл.:    {statistics.stdev(delta_pis):.10f}")
print()

# Квантили
if len(delta_pis) >= 4:
    sorted_deltas = sorted(delta_pis)
    q1_idx = len(sorted_deltas) // 4
    q3_idx = 3 * len(sorted_deltas) // 4
    print(f"  1-й квартиль: {sorted_deltas[q1_idx]:.10f}")
    print(f"  3-й квартиль: {sorted_deltas[q3_idx]:.10f}")
    print()

print("Время вычисления:")
print(f"  Среднее:      {statistics.mean(times):.2f} сек")
print(f"  Минимум:      {min(times):.2f} сек")
print(f"  Максимум:     {max(times):.2f} сек")
print()

# Лучший и худший ключи
best_idx = delta_pis.index(min(delta_pis))
worst_idx = delta_pis.index(max(delta_pis))

print("Лучший ключ (минимальный δπ):")
print(f"  Ключ:  {keys[best_idx]}")
print(f"  δπ:    {delta_pis[best_idx]:.10f}")
print()

print("Худший ключ (максимальный δπ):")
print(f"  Ключ:  {keys[worst_idx]}")
print(f"  δπ:    {delta_pis[worst_idx]:.10f}")
print()

print("Сравнение с эталонами:")
print(f"  AES:         δπ ≈ 0.0156")
print(f"  Кузнечик:    δπ ≈ 0.0078")
print(f"  МиниГОСТ:    δπ ≈ {statistics.mean(delta_pis):.4f} (среднее)")
print()

# Гистограмма
print("Распределение (гистограмма по 0.005):")
bins = {}
for dp in delta_pis:
    bin_key = int(dp / 0.005) * 0.005
    bins[bin_key] = bins.get(bin_key, 0) + 1

for bin_val in sorted(bins.keys()):
    bar = '█' * bins[bin_val]
    print(f"  {bin_val:.3f}-{bin_val+0.005:.3f}: {bar} ({bins[bin_val]})")

print()
print("═" * 70)
EOFANALYSIS

chmod +x "$ANALYSIS_SCRIPT"

echo "Для анализа результатов запустите:"
echo "  python3 $ANALYSIS_SCRIPT $RESULT_FILE"
echo ""

# Запускаем анализ
python3 "$ANALYSIS_SCRIPT" "$RESULT_FILE"
