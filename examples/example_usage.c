#include <stdio.h>
#include <string.h>
#include "minigost.h"

/**
 * Пример использования библиотеки МиниГОСТ в вашей программе
 */

void print_hex(const char *label, const uint8_t *data, size_t length) {
    printf("%s: ", label);
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

int main() {
    printf("════════════════════════════════════════════════════════\n");
    printf("  Пример использования библиотеки МиниГОСТ\n");
    printf("════════════════════════════════════════════════════════\n\n");
    
    // 1. Определяем ключ шифрования (4 байта = 32 бита)
    uint8_t key[4] = {0x12, 0x34, 0x56, 0x78};
    
    printf("Шаг 1: Инициализация ключа\n");
    print_hex("Ключ шифрования", key, 4);
    printf("\n");
    
    // 2. Подготавливаем данные для шифрования
    // Данные должны быть кратны 2 байтам (16 бит - размер блока)
    const char *message = "Hello, MiniGOST!";
    size_t message_len = strlen(message);
    
    // Дополняем до четной длины, если нужно
    size_t padded_len = (message_len % 2 == 0) ? message_len : message_len + 1;
    uint8_t plaintext[padded_len];
    memcpy(plaintext, message, message_len);
    if (message_len % 2 != 0) {
        plaintext[message_len] = 0;  // Дополняем нулем
    }
    
    printf("Шаг 2: Подготовка данных\n");
    printf("Сообщение: \"%s\"\n", message);
    print_hex("Открытый текст", plaintext, padded_len);
    printf("Размер: %zu байт\n\n", padded_len);
    
    // 3. Шифрование
    uint8_t ciphertext[padded_len];
    
    printf("Шаг 3: Шифрование\n");
    int result = minigost_encrypt_ecb(plaintext, ciphertext, padded_len, key);
    if (result != 0) {
        fprintf(stderr, "Ошибка при шифровании!\n");
        return 1;
    }
    print_hex("Зашифрованный текст", ciphertext, padded_len);
    printf("\n");
    
    // 4. Расшифрование
    uint8_t decrypted[padded_len];
    
    printf("Шаг 4: Расшифрование\n");
    result = minigost_decrypt_ecb(ciphertext, decrypted, padded_len, key);
    if (result != 0) {
        fprintf(stderr, "Ошибка при расшифровании!\n");
        return 1;
    }
    print_hex("Расшифрованный текст", decrypted, padded_len);
    printf("Расшифрованное сообщение: \"%s\"\n\n", (char*)decrypted);
    
    // 5. Проверка корректности
    printf("Шаг 5: Проверка\n");
    if (memcmp(plaintext, decrypted, padded_len) == 0) {
        printf("✓ Успех! Расшифрованные данные совпадают с исходными.\n");
    } else {
        printf("✗ Ошибка! Расшифрованные данные не совпадают с исходными.\n");
        return 1;
    }
    
    printf("\n════════════════════════════════════════════════════════\n");
    printf("  Пример работы с отдельными блоками\n");
    printf("════════════════════════════════════════════════════════\n\n");
    
    // Шифрование одного 16-битного блока
    uint16_t block = 0xABCD;
    printf("Исходный блок: 0x%04X\n", block);
    
    uint16_t encrypted_block = minigost_encrypt_block(block, key);
    printf("Зашифрованный блок: 0x%04X\n", encrypted_block);
    
    uint16_t decrypted_block = minigost_decrypt_block(encrypted_block, key);
    printf("Расшифрованный блок: 0x%04X\n", decrypted_block);
    
    if (block == decrypted_block) {
        printf("✓ Блок корректно зашифрован и расшифрован!\n");
    } else {
        printf("✗ Ошибка при работе с блоком!\n");
    }
    
    printf("\n════════════════════════════════════════════════════════\n");
    printf("  Готово!\n");
    printf("════════════════════════════════════════════════════════\n");
    
    return 0;
}
