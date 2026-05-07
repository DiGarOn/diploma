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
