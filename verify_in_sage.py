#!/usr/bin/env sage
# -*- coding: utf-8 -*-
"""
Проверка вычисления линейной характеристики δπ в SageMath

Этот скрипт загружает lookup-таблицу МиниГОСТ из C header файла,
вычисляет корреляцию c(α, β) для указанных масок прямым методом,
и сравнивает с результатами C программы.

Запуск: sage verify_in_sage.py

Требования:
  - SageMath установлен
  - Файл lookup_tables/DEADBEEF_lookup.h существует
"""

from sage.all import *
import re

# ========== Параметры из C программы ==========
KEY = 0xDEADBEEF
ALPHA = 0x7FEF
BETA = 0x4205
EXPECTED_CORRELATION = 0.0253906250

print('╔═══════════════════════════════════════════════════════════════╗')
print('║       Проверка линейной характеристики δπ в SageMath         ║')
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
except Exception as e:
    print(f'✗ Ошибка: {e}')
    print()
    print('Запустите из корня проекта или укажите правильный путь к')
    print('lookup_tables/DEADBEEF_lookup.h')
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

for x in range(65536):
    y = lookup_table[x]  # π(x)
    
    # Вычисляем α·x и β·y
    alpha_dot_x = inner_product(ALPHA, x)
    beta_dot_y = inner_product(BETA, y)
    
    # XOR результатов
    result = alpha_dot_x ^^ beta_dot_y  # ^^ - XOR в Sage
    
    if result == 0:
        count_0 += 1
    else:
        count_1 += 1

# Вычисляем корреляцию
N = 65536
correlation = (count_0 - count_1) / N

print(f'Результаты:')
print(f'  Совпадений (α·x ⊕ β·π(x) = 0): {count_0}')
print(f'  Несовпадений (α·x ⊕ β·π(x) = 1): {count_1}')
print(f'  Всего блоков: {N}')
print()
print(f'Вычисленная корреляция c(α,β) = ({count_0} - {count_1}) / {N}')
print(f'                                = {correlation:.10f}')
print()
print(f'Ожидаемая корреляция (из C):     {EXPECTED_CORRELATION:.10f}')
print()

# ========== Сравнение ==========
difference = abs(correlation - EXPECTED_CORRELATION)
print(f'Разница: {difference:.2e}')
print()

# Проверка с учётом погрешности float32
tolerance = 1e-7  # float32 имеет точность ~7 десятичных знаков

if difference < tolerance:
    print('✓ УСПЕХ: Вычисления совпадают!')
    print('  Корреляция c(α, β) подтверждена.')
else:
    print(f'✗ ОШИБКА: Разница превышает допустимую погрешность ({tolerance})')
    print('  Возможные причины:')
    print('    - Ошибка в загрузке lookup-таблицы')
    print('    - Разные методы вычисления корреляции')
    print('    - Проблема с погрешностью float32/float64')

print()
print('=' * 63)
print()
print('Для полной проверки (поиск максимума, очень долго!) используйте')
print('отдельный скрипт с полным перебором всех α и β.')
