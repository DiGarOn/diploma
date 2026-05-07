/*
 * Вычисление линейной характеристики δπ для МиниГОСТ
 * 
 * Реализует эффективный алгоритм из работы Шишулина Д.И. (стр. 34-36)
 * с использованием предвычисленной lookup-таблицы для ускорения шифрования
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// Подключаем lookup-таблицу МиниГОСТ
#include "lookup_tables/DEADBEEF_lookup.h"

/**
 * Быстрое преобразование Уолша-Адамара (Fast Walsh-Hadamard Transform)
 * 
 * Применяет FWT к вектору длины 65536 для вычисления коэффициентов Фурье.
 * Сложность: O(N log N) где N = 65536
 * 
 * @param f Вектор целых чисел длины 65536 (изменяется на месте)
 */
static void fwt_int_65536(int f[65536]) {
    // Итеративное применение блочной конструкции K
    // 16 уровней для 2^16 = 65536
    for (int len = 1; len < 65536; len <<= 1) {
        for (int start = 0; start < 65536; start += (len << 1)) {
            for (int i = 0; i < len; i++) {
                int a = f[start + i];
                int b = f[start + i + len];
                // Покомпонентные операции
                f[start + i]       = a + b;  // +
                f[start + i + len] = a - b;  // -
            }
        }
    }
}

/**
 * Подсчет чётности (числа единиц по модулю 2)
 * 
 * @param x 16-битное значение для подсчёта чётности
 * @return 1 если нечётное число единиц, 0 если чётное
 */
static inline int parity16(uint16_t x) {
    return __builtin_parity(x);
}

/**
 * Вычисление линейной характеристики δπ
 * 
 * Алгоритм из работы Шишулина Д.И. (стр. 34-36):
 * 
 * 1. Для каждой маски β ≠ 0:
 *    a) Строим булеву функцию g^β(x) = ⊕(βᵢ · yᵢ), где y = π(x)
 *    b) Создаём вектор t длины N: t[x] = g^β(x) преобразованный в {-1, +1}
 *    c) Применяем FWT для получения коэффициентов Фурье
 *    d) Нормируем коэффициенты делением на N
 *    e) Вычисляем корреляции c(α,β) для всех α ≠ 0
 *    f) Находим максимальное |c(α,β)|
 * 
 * 2. Итоговая характеристика:
 *    δπ = max_{α,β≠0} |c(α,β)|
 * 
 * @param lookup_table Lookup-таблица шифрования (plaintext -> ciphertext)
 * @return Линейная характеристика δπ в диапазоне [0, 0.5]
 */
/**
 * Структура для хранения результата вычисления
 */
typedef struct {
    float max_correlation;      // Максимальное преобладание (корреляция)
    uint16_t alpha_max;         // Маска α, где достигается максимум
    uint16_t beta_max;          // Маска β, где достигается максимум
    float correlation_value;    // Знаковое значение корреляции (с учётом знака)
    int max_abs_coeff;          // Максимальный коэффициент Уолша по модулю
    uint64_t max_count;         // Сколько пар (α, β) достигают максимума
    double computation_time;    // Время вычисления в секундах
} DeltaPiResult;

static DeltaPiResult compute_delta_pi_detailed(const uint16_t lookup_table[65536]) {
    DeltaPiResult result = {0};
    const float norm = 1.0f / 65536.0f;
    
    // Выделяем память для FWT (256 MB)
    int *f = (int*)malloc(65536 * sizeof(int));
    if (!f) {
        fprintf(stderr, "Ошибка: не удалось выделить память для FWT\n");
        result.max_correlation = -1.0f;
        return result;
    }
    
    printf("Выделено памяти: 256 MB для FWT буфера\n\n");
    
    clock_t start_time = clock();
    
    // Перебираем все маски β ≠ 0
    for (int beta = 1; beta < 65536; beta++) {
        // Шаг 1: Построение вектора t_β
        for (int x = 0; x < 65536; x++) {
            uint16_t y = lookup_table[x];
            f[x] = parity16(beta & y) ? -1 : 1;
        }
        
        // Шаг 2: Применяем FWT
        fwt_int_65536(f);
        
        // Шаг 3: Нормируем и вычисляем корреляции для всех α ≠ 0
        for (int alpha = 1; alpha < 65536; alpha++) {
            int abs_coeff = abs(f[alpha]);
            // Нормированный коэффициент Фурье (преобладание)
            float correlation = -(float)f[alpha] * norm;
            
            // Обновляем максимум с запоминанием α и β
            if (abs_coeff > result.max_abs_coeff) {
                result.max_abs_coeff = abs_coeff;
                result.max_correlation = (float)abs_coeff * norm;
                result.alpha_max = alpha;
                result.beta_max = beta;
                result.correlation_value = correlation;
                result.max_count = 1;
            } else if (abs_coeff == result.max_abs_coeff) {
                result.max_count++;
            }
        }
        
        // Прогресс (каждые 1024 итераций)
        if (beta % 1024 == 0) {
            clock_t current = clock();
            double elapsed = (double)(current - start_time) / CLOCKS_PER_SEC;
            double progress = (beta * 100.0) / 65535.0;
            double estimated_total = elapsed / progress * 100.0;
            double remaining = estimated_total - elapsed;
            
            printf("\rПрогресс: %.1f%% (β=%d/%d) max=%.6f α=0x%04X β=0x%04X [осталось ~%.0fс]", 
                   progress, beta, 65535, result.max_correlation,
                   result.alpha_max, result.beta_max, remaining);
            fflush(stdout);
        }
    }
    
    clock_t end_time = clock();
    result.computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    printf("\n");
    free(f);
    return result;
}

/**
 * Главная функция программы
 */
int main(int argc, char **argv) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Вычисление линейной характеристики δπ для МиниГОСТ      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Параметры:\n");
    printf("  Ключ:           0x%08X\n", LOOKUP_TABLE_KEY);
    printf("  Размер блока:   16 бит\n");
    printf("  Число блоков:   65536 (полное пространство)\n");
    printf("  Число масок β:  65535 (исключая 0)\n");
    printf("  Число масок α:  65535 (исключая 0)\n\n");
    
    printf("Начинается вычисление...\n");
    printf("Это может занять значительное время!\n\n");
    
    // Вычисляем линейную характеристику с детальной информацией
    DeltaPiResult result = compute_delta_pi_detailed(MINIGOST_LOOKUP_TABLE);
    
    // Выводим результаты
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                      РЕЗУЛЬТАТЫ                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Линейная характеристика δπ (максимальное преобладание):\n");
    printf("  δπ = %.10f\n\n", result.max_correlation);
    
    printf("Линейное соотношение, где достигается максимум:\n");
    printf("  α (маска входа):  0x%04X (%d в десятичной)\n", result.alpha_max, result.alpha_max);
    printf("  β (маска выхода): 0x%04X (%d в десятичной)\n", result.beta_max, result.beta_max);
    printf("  Значение корреляции c(α,β) = %.10f\n", result.correlation_value);
    printf("  |c(α,β)| = %.10f\n", result.max_correlation);
    printf("  Число пар (α,β) с таким максимумом: %llu\n\n", (unsigned long long)result.max_count);
    
    // Расшифровываем биты
    printf("Двоичное представление:\n");
    printf("  α = ");
    for (int i = 15; i >= 0; i--) {
        printf("%d", (result.alpha_max >> i) & 1);
        if (i % 4 == 0 && i > 0) printf(" ");
    }
    printf("\n  β = ");
    for (int i = 15; i >= 0; i--) {
        printf("%d", (result.beta_max >> i) & 1);
        if (i % 4 == 0 && i > 0) printf(" ");
    }
    printf("\n\n");
    
    printf("Интерпретация:\n");
    if (result.max_correlation < 0.01) {
        printf("  ✓ Отлично: Очень низкая линейная характеристика\n");
    } else if (result.max_correlation < 0.05) {
        printf("  ~ Хорошо: Приемлемая линейная характеристика\n");
    } else if (result.max_correlation < 0.1) {
        printf("  ! Средне: Умеренная линейная характеристика\n");
    } else {
        printf("  ✗ Плохо: Высокая линейная характеристика\n");
    }
    
    printf("\nСравнение с эталонами:\n");
    printf("  AES S-box:       δπ ≈ 0.0156 (1/64)\n");
    printf("  ГОСТ «Кузнечик»: δπ ≈ 0.0078 (1/128)\n");
    printf("  МиниГОСТ (ключ 0x%08X): δπ = %.4f\n\n", LOOKUP_TABLE_KEY, result.max_correlation);
    
    printf("Время вычисления: %.2f секунд (%.2f минут)\n\n", 
           result.computation_time, result.computation_time / 60.0);
    
    // Оценка для множества ключей
    printf("Оценка для множества ключей:\n");
    printf("  Время на 1 ключ: %.2f секунд\n", result.computation_time);
    printf("  Для 10 ключей:   %.2f минут\n", result.computation_time * 10 / 60.0);
    printf("  Для 100 ключей:  %.2f часов\n", result.computation_time * 100 / 3600.0);
    printf("  Для 1000 ключей: %.2f дней\n\n", result.computation_time * 1000 / 86400.0);
    
    // Сохраняем детальный результат
    FILE *f = fopen("delta_pi_result.txt", "w");
    if (f) {
        fprintf(f, "Линейная характеристика δπ для МиниГОСТ\n");
        fprintf(f, "========================================\n\n");
        fprintf(f, "Ключ: 0x%08X\n\n", LOOKUP_TABLE_KEY);
        fprintf(f, "Максимальное преобладание (δπ): %.10f\n\n", result.max_correlation);
        fprintf(f, "Линейное соотношение:\n");
        fprintf(f, "  α (маска входа):  0x%04X (%d)\n", result.alpha_max, result.alpha_max);
        fprintf(f, "  β (маска выхода): 0x%04X (%d)\n", result.beta_max, result.beta_max);
        fprintf(f, "  Корреляция c(α,β): %.10f\n", result.correlation_value);
        fprintf(f, "  Число пар (α,β) с максимумом: %llu\n\n", (unsigned long long)result.max_count);
        fprintf(f, "Время вычисления: %.2f секунд (%.2f минут)\n\n", 
                result.computation_time, result.computation_time / 60.0);
        fprintf(f, "Двоичное представление:\n");
        fprintf(f, "  α = ");
        for (int i = 15; i >= 0; i--) {
            fprintf(f, "%d", (result.alpha_max >> i) & 1);
            if (i % 4 == 0 && i > 0) fprintf(f, " ");
        }
        fprintf(f, "\n  β = ");
        for (int i = 15; i >= 0; i--) {
            fprintf(f, "%d", (result.beta_max >> i) & 1);
            if (i % 4 == 0 && i > 0) fprintf(f, " ");
        }
        fprintf(f, "\n");
        fclose(f);
        printf("Детальный результат сохранён в delta_pi_result.txt\n");
    }
    
    // Создаём файл для проверки в SageMath
    FILE *sage_file = fopen("verify_in_sage.sage", "w");
    if (sage_file) {
        fprintf(sage_file, "#!/usr/bin/env sage\n");
        fprintf(sage_file, "# -*- coding: utf-8 -*-\n");
        fprintf(sage_file, "\"\"\"\n");
        fprintf(sage_file, "Проверка вычисления линейной характеристики δπ в SageMath\n");
        fprintf(sage_file, "Для запуска: sage verify_in_sage.py\n");
        fprintf(sage_file, "\"\"\"\n\n");
        fprintf(sage_file, "from sage.all import *\n\n");
        fprintf(sage_file, "# Параметры из C программы\n");
        fprintf(sage_file, "KEY = 0x%08X\n", LOOKUP_TABLE_KEY);
        fprintf(sage_file, "ALPHA = 0x%04X\n", result.alpha_max);
        fprintf(sage_file, "BETA = 0x%04X\n", result.beta_max);
        fprintf(sage_file, "EXPECTED_CORRELATION = %.10f\n\n", result.correlation_value);
        fprintf(sage_file, "print('Проверка вычисления линейной характеристики')\n");
        fprintf(sage_file, "print('='*60)\n");
        fprintf(sage_file, "print(f'Ключ: 0x{KEY:016X}')\n");
        fprintf(sage_file, "print(f'α = 0x{ALPHA:04X}')\n");
        fprintf(sage_file, "print(f'β = 0x{BETA:04X}')\n");
        fprintf(sage_file, "print(f'Ожидаемая корреляция: {EXPECTED_CORRELATION:.10f}')\n");
        fprintf(sage_file, "print()\n\n");
        fprintf(sage_file, "# TODO: Добавьте здесь lookup-таблицу и вычисление корреляции\n");
        fprintf(sage_file, "# См. lookup_tables/DEADBEEFCAFEBABE_lookup.h для таблицы\n");
        fprintf(sage_file, "# Полный скрипт проверки см. в verify_in_sage.py\n");
        fclose(sage_file);
        printf("Скрипт-заглушка для SageMath: verify_in_sage.sage\n");
        printf("Полная проверка: verify_in_sage.py\n\n");
    }
    
    return 0;
}
