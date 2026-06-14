#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Таблица для ускоренного выполнения prep_phase (подстановки + циклический сдвиг)
static const uint8_t PREP_PHASE_TABLE[256] = {
    216,  25,  88, 120,  57,  89, 184, 153,  56, 217, 152, 248, 121, 185,  24, 249,
    200,   9,  72, 104,  41,  73, 168, 137,  40, 201, 136, 232, 105, 169,   8, 233,
    204,  13,  76, 108,  45,  77, 172, 141,  44, 205, 140, 236, 109, 173,  12, 237,
    196,   5,  68, 100,  37,  69, 164, 133,  36, 197, 132, 228, 101, 165,   4, 229,
    212,  21,  84, 116,  53,  85, 180, 149,  52, 213, 148, 244, 117, 181,  20, 245,
    202,  11,  74, 106,  43,  75, 170, 139,  42, 203, 138, 234, 107, 171,  10, 235,
    214,  23,  86, 118,  55,  87, 182, 151,  54, 215, 150, 246, 119, 183,  22, 247,
    210,  19,  82, 114,  51,  83, 178, 147,  50, 211, 146, 242, 115, 179,  18, 243,
    220,  29,  92, 124,  61,  93, 188, 157,  60, 221, 156, 252, 125, 189,  28, 253,
    208,  17,  80, 112,  49,  81, 176, 145,  48, 209, 144, 240, 113, 177,  16, 241,
    218,  27,  90, 122,  59,  91, 186, 155,  58, 219, 154, 250, 123, 187,  26, 251,
    206,  15,  78, 110,  47,  79, 174, 143,  46, 207, 142, 238, 111, 175,  14, 239,
    192,   1,  64,  96,  33,  65, 160, 129,  32, 193, 128, 224,  97, 161,   0, 225,
    198,   7,  70, 102,  39,  71, 166, 135,  38, 199, 134, 230, 103, 167,   6, 231,
    222,  31,  94, 126,  63,  95, 190, 159,  62, 223, 158, 254, 127, 191,  30, 255,
    194,   3,  66,  98,  35,  67, 162, 131,  34, 195, 130, 226,  99, 163,   2, 227
};

/**
 * Основная раундовая функция МиниГОСТ.
 * Выполняет одну итерацию преобразования: g = prep_phase((N1 + k) mod 256)
 *
 * @param n1 Первый 8-битный полублок
 * @param k Ключевой элемент (8 бит)
 * @return Результат преобразования
 */
static inline uint8_t round_function(uint8_t n1, uint8_t k) {
    uint8_t sum = (n1 + k) & 0xFF;  // Сложение по модулю 256
    return PREP_PHASE_TABLE[sum];    // Применение таблицы подстановок
}

/**
 * Одна раундовая функция шифрования МиниГОСТ.
 * В терминах V8 сложение реализуется как XOR:
 * (L, R) -> (R + F(L, K), L) = (R XOR F(L, K), L)
 *
 * @param block Указатель на 16-битный блок (изменяется на месте)
 * @param key_element Ключевой элемент для этого раунда
 */
static inline void minigost_encrypt_round(uint16_t *block, uint8_t key_element) {
    uint8_t n1 = (*block >> 8) & 0xFF;   // Левый полублок (старший байт)
    uint8_t n2 = *block & 0xFF;          // Правый полублок (младший байт)

    // Применяем раундовую функцию к левому полублоку
    uint8_t g = round_function(n1, key_element);

    // Новый левый полублок = Правый XOR F(Левый, K)
    uint8_t new_n1 = n2 ^ g;

    // Новый блок: (R XOR F(L, K), L) = (new_N1, N1)
    *block = ((uint16_t)new_n1 << 8) | n1;
}

/**
 * Обратная раундовая функция для расшифрования МиниГОСТ.
 * В терминах V8 сложение реализуется как XOR:
 * (L, R) -> (R, F(R, K) + L) = (R, F(R, K) XOR L)
 *
 * @param block Указатель на 16-битный блок (изменяется на месте)
 * @param key_element Ключевой элемент для этого раунда
 */
static inline void minigost_decrypt_round(uint16_t *block, uint8_t key_element) {
    uint8_t n1 = (*block >> 8) & 0xFF;   // Левый полублок (старший байт)
    uint8_t n2 = *block & 0xFF;          // Правый полублок (младший байт)

    // Применяем раундовую функцию к правому полублоку
    uint8_t g = round_function(n2, key_element);

    // Новый правый полублок = Левый XOR F(Правый, K)
    uint8_t new_n2 = n1 ^ g;

    // Новый блок: (R, F(R, K) XOR L) = (N2, new_N2)
    *block = ((uint16_t)n2 << 8) | new_n2;
}

/**
 * Шифрование одного 16-битного блока алгоритмом МиниГОСТ.
 * Выполняет 12 раундов преобразования с использованием ключа.
 * 
 * @param plaintext 16-битный блок открытого текста
 * @param key Массив из 4 ключевых элементов (K^(4), K^(3), K^(2), K^(1))
 * @return 16-битный блок зашифрованного текста
 */
uint16_t minigost_encrypt_block(uint16_t plaintext, const uint8_t key[4]) {
    uint16_t block = plaintext;
    static const uint8_t schedule[12] = {3, 2, 1, 0, 3, 2, 1, 0, 0, 1, 2, 3};
    
    // 12 раундов шифрования
    // K = (K^(4), K^(3), K^(2), K^(1))
    // Порядок использования ключей:
    // K^(1), K^(2), K^(3), K^(4), K^(1), K^(2), K^(3), K^(4), K^(4), K^(3), K^(2), K^(1)
    for (int round = 0; round < 12; round++) {
        uint8_t key_element = key[schedule[round]];
        minigost_encrypt_round(&block, key_element);
    }
    
    // Финальная перестановка полублоков (undo последней перестановки раунда)
    uint8_t n1 = (block >> 8) & 0xFF;
    uint8_t n2 = block & 0xFF;
    block = ((uint16_t)n2 << 8) | n1;
    
    return block;
}

/**
 * Расшифрование одного 16-битного блока алгоритмом МиниГОСТ.
 * Выполняет 12 раундов преобразования с использованием ключа в обратном порядке.
 * 
 * @param ciphertext 16-битный блок зашифрованного текста
 * @param key Массив из 4 ключевых элементов (K^(4), K^(3), K^(2), K^(1))
 * @return 16-битный блок открытого текста
 */
uint16_t minigost_decrypt_block(uint16_t ciphertext, const uint8_t key[4]) {
    uint16_t block = ciphertext;
    static const uint8_t schedule[12] = {3, 2, 1, 0, 3, 2, 1, 0, 0, 1, 2, 3};
    
    // Начальная перестановка (обратная финальной при шифровании)
    uint8_t n1 = (block >> 8) & 0xFF;
    uint8_t n2 = block & 0xFF;
    block = ((uint16_t)n2 << 8) | n1;
    
    // 12 раундов расшифрования (обратный порядок ключей, обратная раундовая функция)
    // Используем обратный порядок раундовых ключей
    for (int round = 11; round >= 0; round--) {
        uint8_t key_element = key[schedule[round]];
        minigost_decrypt_round(&block, key_element);
    }
    
    return block;
}

/**
 * Шифрование массива данных в режиме ECB (Electronic Codebook).
 * Каждый 16-битный блок шифруется независимо.
 * 
 * @param plaintext Массив байтов открытого текста
 * @param ciphertext Массив байтов для зашифрованного текста (должен быть того же размера)
 * @param length Длина данных в байтах (должна быть кратна 2)
 * @param key Массив из 4 ключевых элементов
 * @return 0 при успехе, -1 при ошибке
 */
int minigost_encrypt_ecb(const uint8_t *plaintext, uint8_t *ciphertext, 
                         size_t length, const uint8_t key[4]) {
    if (length % 2 != 0) {
        fprintf(stderr, "Error: Data length must be multiple of 2 bytes\n");
        return -1;
    }
    
    for (size_t i = 0; i < length; i += 2) {
        // Собираем 16-битный блок из двух байтов (big-endian)
        uint16_t block = ((uint16_t)plaintext[i] << 8) | plaintext[i + 1];
        
        // Шифруем блок
        uint16_t encrypted = minigost_encrypt_block(block, key);
        
        // Разбиваем результат обратно на байты
        ciphertext[i] = (encrypted >> 8) & 0xFF;
        ciphertext[i + 1] = encrypted & 0xFF;
    }
    
    return 0;
}

/**
 * Расшифрование массива данных в режиме ECB.
 * 
 * @param ciphertext Массив байтов зашифрованного текста
 * @param plaintext Массив байтов для открытого текста (должен быть того же размера)
 * @param length Длина данных в байтах (должна быть кратна 2)
 * @param key Массив из 4 ключевых элементов
 * @return 0 при успехе, -1 при ошибке
 */
int minigost_decrypt_ecb(const uint8_t *ciphertext, uint8_t *plaintext,
                         size_t length, const uint8_t key[4]) {
    if (length % 2 != 0) {
        fprintf(stderr, "Error: Data length must be multiple of 2 bytes\n");
        return -1;
    }
    
    for (size_t i = 0; i < length; i += 2) {
        // Собираем 16-битный блок из двух байтов (big-endian)
        uint16_t block = ((uint16_t)ciphertext[i] << 8) | ciphertext[i + 1];
        
        // Расшифровываем блок
        uint16_t decrypted = minigost_decrypt_block(block, key);
        
        // Разбиваем результат обратно на байты
        plaintext[i] = (decrypted >> 8) & 0xFF;
        plaintext[i + 1] = decrypted & 0xFF;
    }
    
    return 0;
}

// ============================================================================
// ТЕСТОВЫЕ ФУНКЦИИ (только если компилируем с main)
// ============================================================================

#ifndef MINIGOST_NO_MAIN

void print_hex(const char *label, const uint8_t *data, size_t length) {
    printf("%s: ", label);
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

void test_single_block() {
    printf("=== Тест 1: Шифрование одного блока ===\n");
    
    uint16_t plaintext = 0x1234;
    uint8_t key[4] = {0x00, 0x11, 0x22, 0x33};
    
    printf("Открытый текст: 0x%04X\n", plaintext);
    printf("Ключ: ");
    for (int i = 0; i < 4; i++) {
        printf("%02X ", key[i]);
    }
    printf("\n");
    
    uint16_t ciphertext = minigost_encrypt_block(plaintext, key);
    printf("Зашифровано:    0x%04X\n", ciphertext);
    
    uint16_t decrypted = minigost_decrypt_block(ciphertext, key);
    printf("Расшифровано:   0x%04X\n", decrypted);
    
    if (decrypted == plaintext) {
        printf("✓ Тест пройден!\n\n");
    } else {
        printf("✗ Тест не пройден!\n\n");
    }
}

void test_multiple_blocks() {
    printf("=== Тест 2: Шифрование нескольких блоков ===\n");
    
    uint8_t plaintext[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    uint8_t ciphertext[8];
    uint8_t decrypted[8];
    uint8_t key[4] = {0x00, 0x11, 0x22, 0x33};
    
    print_hex("Открытый текст", plaintext, 8);
    print_hex("Ключ", key, 4);
    
    minigost_encrypt_ecb(plaintext, ciphertext, 8, key);
    print_hex("Зашифровано", ciphertext, 8);
    
    minigost_decrypt_ecb(ciphertext, decrypted, 8, key);
    print_hex("Расшифровано", decrypted, 8);
    
    if (memcmp(plaintext, decrypted, 8) == 0) {
        printf("✓ Тест пройден!\n\n");
    } else {
        printf("✗ Тест не пройден!\n\n");
    }
}

void test_prep_phase_table() {
    printf("=== Тест 3: Проверка таблицы PREP_PHASE_TABLE ===\n");
    printf("Проверяем пример из Python: вход 147 (0x93) -> выход 112 (0x70)\n");
    
    uint8_t input = 0x93;  // 147
    uint8_t output = PREP_PHASE_TABLE[input];
    uint8_t expected = 0x70;  // 112
    
    printf("Вход:      0x%02X (%d)\n", input, input);
    printf("Выход:     0x%02X (%d)\n", output, output);
    printf("Ожидалось: 0x%02X (%d)\n", expected, expected);
    
    if (output == expected) {
        printf("✓ Тест пройден!\n\n");
    } else {
        printf("✗ Тест не пройден!\n\n");
    }
}

int main() {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          Тестирование алгоритма МиниГОСТ                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    test_prep_phase_table();
    test_single_block();
    test_multiple_blocks();
    
    printf("Все тесты завершены!\n");
    
    return 0;
}

#endif // MINIGOST_NO_MAIN
