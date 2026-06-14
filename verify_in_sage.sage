#!/usr/bin/env sage
# -*- coding: utf-8 -*-
"""
Проверка вычисления линейной характеристики δπ в SageMath
Для запуска: sage verify_in_sage.py
"""

from sage.all import *

# Параметры из C программы
KEY = 0xDEADBEEF
ALPHA = 0x7FEF
BETA = 0x4205
EXPECTED_CORRELATION = 0.0253906250

print('Проверка вычисления линейной характеристики')
print('='*60)
print(f'Ключ: 0x{KEY:016X}')
print(f'α = 0x{ALPHA:04X}')
print(f'β = 0x{BETA:04X}')
print(f'Ожидаемая корреляция: {EXPECTED_CORRELATION:.10f}')
print()

# TODO: Добавьте здесь lookup-таблицу и вычисление корреляции
# См. lookup_tables/DEADBEEFCAFEBABE_lookup.h для таблицы
# Полный скрипт проверки см. в verify_in_sage.py
