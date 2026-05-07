#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minigost.h"

static int parse_hex32(const char *value, uint32_t *result) {
    char *end = NULL;
    const char *digits = value;
    unsigned long parsed = 0;

    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        digits = value + 2;
    }

    errno = 0;
    parsed = strtoul(digits, &end, 16);
    if (errno != 0 || end == digits || *end != '\0' || parsed > 0xFFFFFFFFul) {
        return -1;
    }

    *result = (uint32_t)parsed;
    return 0;
}

static int parse_hex16(const char *value, uint16_t *result) {
    uint32_t parsed = 0;

    if (parse_hex32(value, &parsed) != 0 || parsed > 0xFFFFu) {
        return -1;
    }

    *result = (uint16_t)parsed;
    return 0;
}

static void split_key_be(uint32_t raw_key, uint8_t key[4]) {
    key[0] = (uint8_t)((raw_key >> 24) & 0xFFu);
    key[1] = (uint8_t)((raw_key >> 16) & 0xFFu);
    key[2] = (uint8_t)((raw_key >> 8) & 0xFFu);
    key[3] = (uint8_t)(raw_key & 0xFFu);
}

static void trim_line(char *line) {
    size_t length = strlen(line);
    while (length > 0 && isspace((unsigned char)line[length - 1])) {
        line[--length] = '\0';
    }
}

int main(int argc, char **argv) {
    uint32_t raw_key = 0;
    uint8_t key[4];
    char line[128];

    if (argc != 3) {
        fprintf(
            stderr,
            "Usage: %s <encrypt|decrypt> <32-bit-key>\n"
            "Example: %s encrypt 0xA1B2C3D4\n",
            argv[0],
            argv[0]
        );
        return 1;
    }

    if (strcmp(argv[1], "encrypt") != 0 && strcmp(argv[1], "decrypt") != 0) {
        fprintf(stderr, "Unknown mode: %s\n", argv[1]);
        return 1;
    }

    if (parse_hex32(argv[2], &raw_key) != 0) {
        fprintf(stderr, "Invalid 32-bit key: %s\n", argv[2]);
        return 1;
    }

    split_key_be(raw_key, key);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        uint16_t block = 0;
        uint16_t result = 0;

        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        if (parse_hex16(line, &block) != 0) {
            fprintf(stderr, "Invalid 16-bit block: %s\n", line);
            return 2;
        }

        if (strcmp(argv[1], "encrypt") == 0) {
            result = minigost_encrypt_block(block, key);
        } else {
            result = minigost_decrypt_block(block, key);
        }

        printf("%04" PRIX16 "\n", result);
    }

    if (ferror(stdin)) {
        fprintf(stderr, "Failed to read stdin\n");
        return 3;
    }

    return 0;
}
