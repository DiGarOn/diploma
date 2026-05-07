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
    int max_abs_coeff;
    uint64_t max_count;
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
            int abs_coeff = abs(f[alpha]);
            float corr = -(float)f[alpha] * norm;
            
            if (abs_coeff > result.max_abs_coeff) {
                result.max_abs_coeff = abs_coeff;
                result.delta_pi = (float)abs_coeff * norm;
                result.alpha = alpha;
                result.beta = beta;
                result.correlation = corr;
                result.max_count = 1;
            } else if (abs_coeff == result.max_abs_coeff) {
                result.max_count++;
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
    printf("0x%08X,%.10f,0x%04X,0x%04X,%.10f,%llu,%.2f\n",
           key, result.delta_pi, result.alpha, result.beta, 
           result.correlation, (unsigned long long)result.max_count, result.time_sec);
    
    return 0;
}
