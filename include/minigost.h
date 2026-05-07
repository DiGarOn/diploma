#ifndef MINIGOST_H
#define MINIGOST_H

#include <stdint.h>
#include <stddef.h>

/**
 * Заголовочный файл для библиотеки шифрования МиниГОСТ
 * 
 * МиниГОСТ - упрощенный алгоритм блочного шифрования на основе ГОСТ/Магма.
 * Размер блока: 16 бит
 * Размер ключа: 32 бита (4 элемента по 8 бит)
 * Количество раундов: 12
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Шифрование одного 16-битного блока алгоритмом МиниГОСТ.
 * 
 * @param plaintext 16-битный блок открытого текста
 * @param key Массив из 4 ключевых элементов (K^(4), K^(3), K^(2), K^(1))
 * @return 16-битный блок зашифрованного текста
 */
uint16_t minigost_encrypt_block(uint16_t plaintext, const uint8_t key[4]);

/**
 * Расшифрование одного 16-битного блока алгоритмом МиниГОСТ.
 * 
 * @param ciphertext 16-битный блок зашифрованного текста
 * @param key Массив из 4 ключевых элементов (K^(4), K^(3), K^(2), K^(1))
 * @return 16-битный блок открытого текста
 */
uint16_t minigost_decrypt_block(uint16_t ciphertext, const uint8_t key[4]);

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
                         size_t length, const uint8_t key[4]);

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
                         size_t length, const uint8_t key[4]);

#ifdef __cplusplus
}
#endif

#endif // MINIGOST_H
