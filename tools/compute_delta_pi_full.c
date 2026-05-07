/**
 * Полное вычисление δπ с сохранением ВСЕХ преобладаний для проверки в SageMath
 * 
 * Компиляция:
 *   gcc -O3 -march=native -o build/compute_delta_pi_full tools/compute_delta_pi_full.c -lm
 * 
 * Запуск:
 *   ./build/compute_delta_pi_full
 * 
 * ВНИМАНИЕ: Создаёт файл размером ~17 GB (65535*65535*4 байта)!
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "lookup_tables/DEADBEEFCAFEBABE_lookup.h"

// FWT для 16-бит (65536 элементов)
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

// Вычисление четности для 16-бит
static inline int parity16(uint16_t x) {
    return __builtin_parity(x);
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║   ПОЛНОЕ ВЫЧИСЛЕНИЕ δπ с сохранением всех преобладаний    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("⚠ ВНИМАНИЕ: Будет создан файл размером ~17 GB!\n");
    printf("  Это матрица 65535 × 65535 корреляций (float)\n");
    printf("  Убедитесь, что у вас достаточно места на диске.\n\n");
    
    printf("Продолжить? (y/n): ");
    char response;
    scanf(" %c", &response);
    if (response != 'y' && response != 'Y') {
        printf("Отменено.\n");
        return 0;
    }
    
    printf("\nНачинаю вычисления...\n\n");
    
    const float norm = 1.0f / 65536.0f;
    float max_correlation = 0.0f;
    uint16_t alpha_max = 0, beta_max = 0;
    float correlation_value_max = 0.0f;
    
    // Открываем файл для записи матрицы корреляций
    FILE *matrix_file = fopen("results/correlation_matrix.bin", "wb");
    if (!matrix_file) {
        fprintf(stderr, "Ошибка: не удалось создать файл корреляций\n");
        return 1;
    }
    
    // Выделяем память для FWT
    int *f = (int*)malloc(65536 * sizeof(int));
    if (!f) {
        fprintf(stderr, "Ошибка выделения памяти\n");
        fclose(matrix_file);
        return 1;
    }
    
    printf("Ключ: 0x%016llX\n\n", LOOKUP_TABLE_KEY);
    
    clock_t start_time = clock();
    
    // Перебираем все маски β ≠ 0
    for (int beta = 1; beta < 65536; beta++) {
        // Построение вектора t_β
        for (int x = 0; x < 65536; x++) {
            uint16_t y = MINIGOST_LOOKUP_TABLE[x];
            f[x] = parity16(beta & y) ? -1 : 1;
        }
        
        // FWT
        fwt_int_65536(f);
        
        // Буфер для записи строки корреляций (65535 значений, пропускаем α=0)
        float *row = (float*)malloc(65535 * sizeof(float));
        if (!row) {
            fprintf(stderr, "Ошибка выделения памяти для строки\n");
            free(f);
            fclose(matrix_file);
            return 1;
        }
        
        // Вычисляем корреляции для всех α ≠ 0 и сохраняем
        for (int alpha = 1; alpha < 65536; alpha++) {
            float correlation = -(float)f[alpha] * norm;
            float abs_corr = fabsf(correlation);
            
            row[alpha - 1] = correlation;  // Сохраняем знаковое значение
            
            if (abs_corr > max_correlation) {
                max_correlation = abs_corr;
                alpha_max = alpha;
                beta_max = beta;
                correlation_value_max = correlation;
            }
        }
        
        // Записываем строку в файл
        fwrite(row, sizeof(float), 65535, matrix_file);
        free(row);
        
        // Прогресс
        if (beta % 1024 == 0) {
            clock_t current = clock();
            double elapsed = (double)(current - start_time) / CLOCKS_PER_SEC;
            double progress = (beta * 100.0) / 65535.0;
            double estimated_total = elapsed / progress * 100.0;
            double remaining = estimated_total - elapsed;
            
            printf("\rПрогресс: %.1f%% (β=%d/%d) max=%.6f [~%.0fс осталось]", 
                   progress, beta, 65535, max_correlation, remaining);
            fflush(stdout);
        }
    }
    
    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    printf("\n\n");
    free(f);
    fclose(matrix_file);
    
    // Выводим результаты
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                      РЕЗУЛЬТАТЫ                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Максимальное преобладание δπ = %.10f\n", max_correlation);
    printf("Достигается при:\n");
    printf("  α = 0x%04X (%d)\n", alpha_max, alpha_max);
    printf("  β = 0x%04X (%d)\n", beta_max, beta_max);
    printf("  c(α,β) = %.10f\n\n", correlation_value_max);
    
    printf("Время вычисления: %.2f секунд (%.2f минут)\n\n", elapsed, elapsed / 60.0);
    
    printf("Файлы:\n");
    printf("  results/correlation_matrix.bin - матрица корреляций (17 GB)\n");
    printf("  Формат: 65535 строк × 65535 столбцов (float32)\n");
    printf("  Индексация: row[β-1][α-1] = c(α, β)\n\n");
    
    // Создаём метаданные для SageMath
    FILE *meta = fopen("results/correlation_matrix.meta", "w");
    if (meta) {
        fprintf(meta, "KEY=0x%016llX\n", LOOKUP_TABLE_KEY);
        fprintf(meta, "DELTA_PI=%.10f\n", max_correlation);
        fprintf(meta, "ALPHA_MAX=0x%04X\n", alpha_max);
        fprintf(meta, "BETA_MAX=0x%04X\n", beta_max);
        fprintf(meta, "CORRELATION_VALUE=%.10f\n", correlation_value_max);
        fprintf(meta, "COMPUTATION_TIME=%.2f\n", elapsed);
        fprintf(meta, "MATRIX_ROWS=65535\n");
        fprintf(meta, "MATRIX_COLS=65535\n");
        fprintf(meta, "ELEMENT_SIZE=4\n");
        fprintf(meta, "TOTAL_SIZE=%lld\n", (long long)(65535LL * 65535LL * 4LL));
        fclose(meta);
        printf("  results/correlation_matrix.meta - метаданные\n\n");
    }
    
    // Создаём скрипт для проверки в SageMath
    FILE *sage = fopen("verify_correlation.sage", "w");
    if (sage) {
        fprintf(sage, "#!/usr/bin/env sage\n");
        fprintf(sage, "# -*- coding: utf-8 -*-\n");
        fprintf(sage, "\"\"\"\n");
        fprintf(sage, "Проверка корреляций из файла correlation_matrix.bin\n");
        fprintf(sage, "Запуск: sage verify_correlation.sage\n");
        fprintf(sage, "\"\"\"\n\n");
        fprintf(sage, "import struct\n");
        fprintf(sage, "import numpy as np\n\n");
        fprintf(sage, "# Метаданные\n");
        fprintf(sage, "KEY = 0x%016llX\n", LOOKUP_TABLE_KEY);
        fprintf(sage, "ALPHA_MAX = 0x%04X\n", alpha_max);
        fprintf(sage, "BETA_MAX = 0x%04X\n", beta_max);
        fprintf(sage, "EXPECTED_CORRELATION = %.10f\n\n", correlation_value_max);
        fprintf(sage, "print('Загрузка матрицы корреляций...')\n");
        fprintf(sage, "with open('results/correlation_matrix.bin', 'rb') as f:\n");
        fprintf(sage, "    data = f.read()\n");
        fprintf(sage, "    n_elements = len(data) // 4\n");
        fprintf(sage, "    correlations = struct.unpack(f'{n_elements}f', data)\n");
        fprintf(sage, "    matrix = np.array(correlations).reshape(65535, 65535)\n\n");
        fprintf(sage, "print(f'Матрица загружена: {matrix.shape}')\n");
        fprintf(sage, "print(f'Проверяем c({ALPHA_MAX:#06x}, {BETA_MAX:#06x})...')\n\n");
        fprintf(sage, "# Индексация: row[β-1][α-1]\n");
        fprintf(sage, "computed_value = matrix[BETA_MAX - 1, ALPHA_MAX - 1]\n");
        fprintf(sage, "print(f'Из файла:   c(α,β) = {computed_value:.10f}')\n");
        fprintf(sage, "print(f'Ожидалось:  c(α,β) = {EXPECTED_CORRELATION:.10f}')\n");
        fprintf(sage, "print(f'Разница:    {abs(computed_value - EXPECTED_CORRELATION):.2e}')\n\n");
        fprintf(sage, "# Проверяем максимум\n");
        fprintf(sage, "abs_matrix = np.abs(matrix)\n");
        fprintf(sage, "max_idx = np.unravel_index(np.argmax(abs_matrix), abs_matrix.shape)\n");
        fprintf(sage, "max_value = abs_matrix[max_idx]\n");
        fprintf(sage, "max_beta = max_idx[0] + 1\n");
        fprintf(sage, "max_alpha = max_idx[1] + 1\n");
        fprintf(sage, "print(f'\\nМаксимум в матрице:')\n");
        fprintf(sage, "print(f'  |c(0x{max_alpha:04x}, 0x{max_beta:04x})| = {max_value:.10f}')\n");
        fprintf(sage, "print(f'Совпадает с C программой: {max_alpha == ALPHA_MAX and max_beta == BETA_MAX}')\n");
        fclose(sage);
        printf("  verify_correlation.sage - скрипт проверки для SageMath\n\n");
    }
    
    printf("✓ Готово!\n\n");
    printf("Для проверки в SageMath:\n");
    printf("  sage verify_correlation.sage\n\n");
    
    return 0;
}
