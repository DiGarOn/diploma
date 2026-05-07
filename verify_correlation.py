#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Проверка вычисления линейной характеристики δπ (Python версия)

Этот скрипт не требует SageMath - работает на стандартном Python 3.

Запуск: python3 verify_correlation.py
"""

import re
import sys

# ========== Параметры из C программы ==========
KEY = 0xDEADBEEF
ALPHA = 0xEF7F
BETA = 0x0542
EXPECTED_CORRELATION = 0.0253906250

print('╔═══════════════════════════════════════════════════════════════╗')
print('║       Проверка линейной характеристики δπ (Python)           ║')
print('╚═══════════════════════════════════════════════════════════════╝')
print()
print(f'Ключ шифрования: 0x{KEY:08X}')
print(f'Маска входа  α:  0x{ALPHA:04X} = {bin(ALPHA)[2:].zfill(16)}')
print(f'Маска выхода β:  0x{BETA:04X} = {bin(BETA)[2:].zfill(16)}')
print(f'Ожидаемое значение c(α,β): {EXPECTED_CORRELATION:.10f}')
print()

# ========== Загрузка lookup-таблицы из C файла ==========
print('Загрузка lookup-таблицы из C header файла...')
lookup_table = [0] * 65536

try:
    with open('lookup_tables/DEADBEEF_lookup.h', 'r') as f:
        content = f.read()
        
        # Ищем массив: uint16_t MINIGOST_LOOKUP_TABLE[65536] = { ... };
        match = re.search(r'uint16_t\s+MINIGOST_LOOKUP_TABLE\[65536\]\s*=\s*\{([^}]+)\}', content, re.DOTALL)
        if not match:
            raise ValueError("Не удалось найти массив MINIGOST_LOOKUP_TABLE")
        
        # Извлекаем все числа (hex или decimal)
        numbers = re.findall(r'0x[0-9a-fA-F]+|\d+', match.group(1))
        
        if len(numbers) != 65536:
            raise ValueError(f"Найдено {len(numbers)} элементов вместо 65536")
        
        for i, num_str in enumerate(numbers):
            if num_str.startswith('0x'):
                lookup_table[i] = int(num_str, 16)
            else:
                lookup_table[i] = int(num_str)
    
    print(f'✓ Загружено {len(lookup_table)} элементов')
    print(f'  Примеры: π(0x0000) = 0x{lookup_table[0]:04X}')
    print(f'           π(0xFFFF) = 0x{lookup_table[65535]:04X}')
    print()
except FileNotFoundError:
    print('✗ Ошибка: файл lookup_tables/DEADBEEF_lookup.h не найден')
    print()
    print('Сначала сгенерируйте lookup-таблицу:')
    print('  ./generate_lookup.sh')
    print()
    sys.exit(1)
except Exception as e:
    print(f'✗ Ошибка: {e}')
    print()
    sys.exit(1)

# ========== Вычисление корреляции c(α, β) ==========
print(f'Вычисление корреляции c(α={ALPHA:#06x}, β={BETA:#06x})...')
print('Формула: c(α, β) = (#{x : α·x ⊕ β·π(x) = 0} - #{x : α·x ⊕ β·π(x) = 1}) / N')
print()

def parity(x):
    """Вычисляет четность (сумму битов по модулю 2)"""
    return bin(x).count('1') % 2

def inner_product(a, b):
    """Скалярное произведение (побитовое AND + четность)"""
    return parity(a & b)

# Подсчёт совпадений/несовпадений
count_0 = 0  # Число x, где α·x ⊕ β·π(x) = 0
count_1 = 0  # Число x, где α·x ⊕ β·π(x) = 1

print('Перебор всех 65536 значений x...')
for x in range(65536):
    y = lookup_table[x]  # π(x)
    
    # Вычисляем α·x и β·y
    alpha_dot_x = inner_product(ALPHA, x)
    beta_dot_y = inner_product(BETA, y)
    
    # XOR результатов
    result = alpha_dot_x ^ beta_dot_y
    
    if result == 0:
        count_0 += 1
    else:
        count_1 += 1
    
    # Прогресс
    if x % 8192 == 0 and x > 0:
        print(f'  Обработано: {x}/65536 ({x*100//65536}%)')

print(f'  Обработано: 65536/65536 (100%)')
print()

# Вычисляем корреляцию
N = 65536
correlation = (count_0 - count_1) / N

print('=' * 63)
print('РЕЗУЛЬТАТЫ:')
print('=' * 63)
print()
print(f'Совпадений (α·x ⊕ β·π(x) = 0):  {count_0:5d}  ({count_0/N*100:.2f}%)')
print(f'Несовпадений (α·x ⊕ β·π(x) = 1): {count_1:5d}  ({count_1/N*100:.2f}%)')
print(f'Всего блоков:                   {N:5d}')
print()
print(f'Вычисленная корреляция c(α,β):')
print(f'  c(α,β) = ({count_0} - {count_1}) / {N}')
print(f'         = {correlation:.10f}')
print()
print(f'Ожидаемая корреляция (из C программы):')
print(f'         = {EXPECTED_CORRELATION:.10f}')
print()

# ========== Сравнение ==========
difference = abs(abs(correlation) - abs(EXPECTED_CORRELATION))
print(f'Разница (по модулю): {difference:.2e}')
print()

# Проверка с учётом погрешности float32
tolerance = 1e-7  # float32 имеет точность ~7 десятичных знаков

print('=' * 63)
if difference < tolerance:
    print('✓✓✓ УСПЕХ: Вычисления совпадают! ✓✓✓')
    print()
    print('Корреляция c(α, β) подтверждена независимым вычислением.')
    print('Программа на C работает корректно.')
    print()
    if correlation * EXPECTED_CORRELATION < 0:
        print('Примечание: Знак корреляции различается, но это нормально.')
        print('Для линейного криптоанализа важен модуль |c(α,β)|.')
else:
    print('✗✗✗ ОШИБКА ✗✗✗')
    print()
    print(f'Разница {difference:.2e} превышает допустимую погрешность {tolerance}')
    print()
    print('Возможные причины:')
    print('  - Ошибка в загрузке lookup-таблицы')
    print('  - Разные методы вычисления корреляции')
    print('  - Проблема с погрешностью float32/float64')

print('=' * 63)
print()

# Дополнительная информация
print('ИНТЕРПРЕТАЦИЯ:')
print()
print(f'Вероятность успеха линейного приближения:')
print(f'  p = 0.5 + c(α,β)/2 = 0.5 + {correlation/2:.6f} = {0.5 + correlation/2:.6f}')
print(f'  Отклонение от случайного: {abs(correlation)*100:.2f}%')
print()
print('Сравнение с эталонами:')
print('  AES S-box:       δπ ≈ 0.0156 (1/64)')
print('  ГОСТ «Кузнечик»: δπ ≈ 0.0078 (1/128)')
print(f'  МиниГОСТ:        δπ = {abs(correlation):.4f} (1/{1/abs(correlation):.1f})')
print()
print('=' * 63)
