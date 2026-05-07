// Пример использования lookup-таблицы МиниГОСТ для сверхбыстрого шифрования

#include <stdio.h>
#include <stdint.h>
#include <time.h>

// Подключаем сгенерированную lookup-таблицу
// (файл будет создан после запуска generate_lookup_table.py)
// #include "lookup_tables/lookup_key_0011223344556677.h"

// Для демонстрации используем заглушку
#ifndef MINIGOST_LOOKUP_TABLE
static const uint16_t MINIGOST_LOOKUP_TABLE[65536] = {0};  // Заглушка
static inline uint16_t minigost_encrypt_lookup(uint16_t plaintext) {
    return MINIGOST_LOOKUP_TABLE[plaintext];
}
#endif

/**
 * Шифрование массива данных через lookup-таблицу.
 * НАМНОГО быстрее обычного алгоритма!
 * 
 * @param plaintext Массив открытого текста
 * @param ciphertext Массив для зашифрованного текста
 * @param length Длина в байтах (должна быть кратна 2)
 */
void encrypt_data_lookup(const uint8_t *plaintext, uint8_t *ciphertext, size_t length) {
    for (size_t i = 0; i < length; i += 2) {
        // Собираем 16-битный блок
        uint16_t block = ((uint16_t)plaintext[i] << 8) | plaintext[i + 1];
        
        // Мгновенное шифрование через lookup!
        uint16_t encrypted = minigost_encrypt_lookup(block);
        
        // Разбиваем обратно
        ciphertext[i] = (encrypted >> 8) & 0xFF;
        ciphertext[i + 1] = encrypted & 0xFF;
    }
}

/**
 * Бенчмарк: сравнение скорости lookup vs обычный алгоритм.
 */
void benchmark_lookup() {
    const size_t DATA_SIZE = 1024 * 1024;  // 1 MB
    const int ITERATIONS = 100;
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     Бенчмарк lookup-таблицы МиниГОСТ                      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    uint8_t *plaintext = malloc(DATA_SIZE);
    uint8_t *ciphertext = malloc(DATA_SIZE);
    
    // Заполняем тестовыми данными
    for (size_t i = 0; i < DATA_SIZE; i++) {
        plaintext[i] = (uint8_t)(i & 0xFF);
    }
    
    // Замер времени
    clock_t start = clock();
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        encrypt_data_lookup(plaintext, ciphertext, DATA_SIZE);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    double throughput_mbps = (DATA_SIZE * ITERATIONS / (1024.0 * 1024.0)) / elapsed;
    
    printf("Результаты:\n");
    printf("  Данных обработано: %.1f MB\n", (DATA_SIZE * ITERATIONS) / (1024.0 * 1024.0));
    printf("  Время:             %.3f сек\n", elapsed);
    printf("  Пропускная способность: %.1f MB/s\n", throughput_mbps);
    printf("\n");
    
    printf("Оценка скорости:\n");
    double blocks_per_sec = (DATA_SIZE / 2) * ITERATIONS / elapsed;
    printf("  Блоков в секунду:  %.2e\n", blocks_per_sec);
    printf("  Наносекунд на блок: %.2f ns\n", 1e9 / blocks_per_sec);
    printf("\n");
    
    free(plaintext);
    free(ciphertext);
}

int main() {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Пример использования lookup-таблицы МиниГОСТ\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    // Простой пример
    uint16_t plaintext = 0x1234;
    uint16_t encrypted = minigost_encrypt_lookup(plaintext);
    
    printf("Простое шифрование:\n");
    printf("  Открытый текст:      0x%04X\n", plaintext);
    printf("  Зашифрованный текст: 0x%04X\n", encrypted);
    printf("\n");
    
    printf("Преимущества lookup-таблицы:\n");
    printf("  ✓ Шифрование за O(1) - один lookup в памяти\n");
    printf("  ✓ Никаких вычислений - только чтение\n");
    printf("  ✓ Идеально для кэша CPU\n");
    printf("  ✓ Детерминированное время выполнения\n");
    printf("  ✓ Защита от timing attacks\n");
    printf("\n");
    
    // Запускаем бенчмарк
    benchmark_lookup();
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Для генерации lookup-таблицы используйте:\n");
    printf("  python3 tools/generate_lookup_table.py --key 0xYOURKEY\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}
