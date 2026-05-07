#!/bin/bash
#
# Скрипт для компиляции и запуска вычисления δπ
#

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Компиляция программы вычисления δπ для МиниГОСТ          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Проверяем наличие lookup-таблицы
if [ ! -f "lookup_tables/DEADBEEF_lookup.h" ]; then
    echo "❌ Ошибка: lookup-таблица не найдена!"
    echo ""
    echo "Сначала сгенерируйте lookup-таблицу:"
    echo "  ./generate_lookup.sh"
    echo ""
    exit 1
fi

echo "✓ Lookup-таблица найдена"
echo ""

# Компилируем с оптимизацией
echo "Компиляция с оптимизацией -O3..."
gcc -O3 -march=native -Wall -Wextra \
    -I. \
    -o build/compute_delta_pi \
    tools/compute_delta_pi.c \
    -lm

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Ошибка компиляции!"
    exit 1
fi

echo "✓ Компиляция успешна"
echo ""

# Создаём директорию для результатов
mkdir -p results

echo "╔════════════════════════════════════════════════════════════╗"
echo "║              Запуск вычисления δπ                          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Оценка времени выполнения: ~6 секунд"
echo "Использует ~256 MB памяти"
echo ""
echo "Прогресс будет выводиться в реальном времени."
echo ""

read -p "Продолжить? [y/N] " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Отменено."
    exit 0
fi

echo ""
echo "Запуск программы..."
echo ""

# Запускаем
time ./build/compute_delta_pi

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                    ЗАВЕРШЕНО                               ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

if [ -f "delta_pi_result.txt" ]; then
    mv delta_pi_result.txt results/
    echo "✓ Результат сохранён в results/delta_pi_result.txt"
fi

echo ""
