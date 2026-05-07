/*
 * Тестовая версия вычисления δπ для МиниГОСТ
 * Вычисляет только для первых 256 масок β для быстрой проверки
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "lookup_tables/DEADBEEFCAFEBABE_lookup.h"

static void fwt_int_65536(int f[65536]) {
    for (int len = 1; len < 65536; len <<= 1) {
        for (int start = 0; start < 65536; start += (len << 1)) {
            for (int i = 0; i < len; i++) {
                int a = f[start + i];
                int b = f[start + i + len];
                f[start + i]       = a + b;
                f[start + i + len] = a - b;
            }
        }
    }
}

static inline int parity16(uint16_t x) {
    return __builtin_parity(x);
}

typedef struct {
    float max_correlation;
    uint16_t alpha_max;
    uint16_t beta_max;
    float correlation_value;
} TestResult;

static TestResult compute_delta_pi_test(const uint16_t lookup_table[65536], int max_beta) {
    TestResult result = {0};
    const float norm = 1.0f / 65536.0f;
    
    int *f = (int*)malloc(65536 * sizeof(int));
    if (!f) {
        fprintf(stderr, "Ошибка выделения памяти\n");
        result.max_correlation = -1.0f;
        return result;
    }
    
    for (int beta = 1; beta < max_beta; beta++) {
        for (int x = 0; x < 65536; x++) {
            uint16_t y = lookup_table[x];
            f[x] = parity16(beta & y) ? -1 : 1;
        }
        
        fwt_int_65536(f);
        
        for (int alpha = 1; alpha < 65536; alpha++) {
            float correlation = -(float)f[alpha] * norm;
            float abs_corr = fabsf(correlation);
            if (abs_corr > result.max_correlation) {
                result.max_correlation = abs_corr;
                result.alpha_max = alpha;
                result.beta_max = beta;
                result.correlation_value = correlation;
            }
        }
        
        if (beta % 16 == 0) {
            printf("\rПрогресс: %d/%d (max=%.6f α=0x%04X β=0x%04X)", 
                   beta, max_beta - 1, result.max_correlation, 
                   result.alpha_max, result.beta_max);
            fflush(stdout);
        }
    }
    
    printf("\n");
    free(f);
    return result;
}

int main() {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     ТЕСТ: Вычисление δπ (первые 256 масок β)             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Ключ: 0x%016llX\n\n", LOOKUP_TABLE_KEY);
    
    printf("Быстрый тест (β = 1..255):\n");
    clock_t start = clock();
    TestResult result = compute_delta_pi_test(MINIGOST_LOOKUP_TABLE, 256);
    clock_t end = clock();
    
    printf("\nРезультат теста:\n");
    printf("  δπ (первые 255 масок) = %.10f\n", result.max_correlation);
    printf("  α = 0x%04X (%d)\n", result.alpha_max, result.alpha_max);
    printf("  β = 0x%04X (%d)\n", result.beta_max, result.beta_max);
    printf("  Корреляция c(α,β) = %.10f\n", result.correlation_value);
    printf("  Время: %.2f сек\n", (double)(end - start) / CLOCKS_PER_SEC);
    printf("\n");
    printf("Это оценка снизу. Полное значение будет >= %.10f\n", result.max_correlation);
    printf("\nДля полного вычисления запустите: ./run_delta_pi.sh\n\n");
    
    return 0;
}
