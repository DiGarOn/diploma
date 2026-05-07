#!/usr/bin/env python3
"""
Генератор lookup-таблиц для МиниГОСТ.

Для заданного ключа генерирует полную таблицу шифрования:
    encrypted_block = LOOKUP_TABLE[plaintext_block]

Это превращает алгоритм в простой lookup O(1) вместо вычислений O(n).
"""

from __future__ import annotations

import argparse
import sys
import struct
from pathlib import Path

# Добавляем корневую директорию в путь для импорта minigost
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from minigost import MiniGost


def generate_lookup_table(key: int) -> list[int]:
    """
    Генерирует полную lookup-таблицу для заданного ключа.
    
    Args:
        key: 32-битный ключ
        
    Returns:
        Список из 65536 элементов: lookup_table[plaintext] = ciphertext
    """
    cipher = MiniGost()
    lookup_table = []
    
    print(f"Генерация таблицы для ключа 0x{key:016X}...")
    print("Прогресс: ", end="", flush=True)
    
    for block in range(0x10000):  # 0x0000..0xFFFF
        encrypted = cipher.encrypt_block(block, key)
        lookup_table.append(encrypted)
        
        # Показываем прогресс каждые 4096 блоков
        if (block + 1) % 4096 == 0:
            progress = (block + 1) * 100 // 0x10000
            print(f"{progress}%... ", end="", flush=True)
    
    print("100% ✓")
    return lookup_table


def save_as_binary(lookup_table: list[int], key: int, output_path: Path) -> None:
    """
    Сохраняет таблицу в бинарном формате (компактно, быстро).
    
    Формат файла:
    - 4 байта: ключ (uint32_t, little-endian)
    - 65536 * 2 байт: таблица (uint16_t[], little-endian)
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'wb') as f:
        # Записываем ключ
        f.write(struct.pack('<I', key))
        
        # Записываем таблицу
        for value in lookup_table:
            f.write(struct.pack('<H', value))
    
    size_kb = output_path.stat().st_size / 1024
    print(f"Сохранено в бинарном формате: {output_path} ({size_kb:.1f} KB)")


def save_as_c_header(lookup_table: list[int], key: int, output_path: Path) -> None:
    """
    Сохраняет таблицу как C заголовочный файл для прямого включения в код.
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        f.write(f"""// Автоматически сгенерированная lookup-таблица для МиниГОСТ
// Ключ: 0x{key:08X}
// Размер: 65536 записей по 2 байта = 128 KB

#ifndef MINIGOST_LOOKUP_TABLE_H
#define MINIGOST_LOOKUP_TABLE_H

#include <stdint.h>

// Ключ, для которого сгенерирована таблица
#define LOOKUP_TABLE_KEY 0x{key:08X}U

// Таблица шифрования: LOOKUP_TABLE[plaintext] = ciphertext
static const uint16_t MINIGOST_LOOKUP_TABLE[65536] = {{
""")
        
        # Записываем таблицу по 8 значений в строке
        for i in range(0, len(lookup_table), 8):
            values = lookup_table[i:i+8]
            line = "    " + ", ".join(f"0x{v:04X}" for v in values)
            if i + 8 < len(lookup_table):
                line += ","
            f.write(line + "\n")
        
        f.write("""};

// Функция шифрования через lookup
static inline uint16_t minigost_encrypt_lookup(uint16_t plaintext) {
    return MINIGOST_LOOKUP_TABLE[plaintext];
}

#endif // MINIGOST_LOOKUP_TABLE_H
""")
    
    size_kb = output_path.stat().st_size / 1024
    print(f"Сохранено как C header: {output_path} ({size_kb:.1f} KB)")


def save_as_markdown(lookup_table: list[int], key: int, output_path: Path) -> None:
    """
    Сохраняет таблицу в Markdown формате для удобного чтения.
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        f.write(f"# Lookup-таблица МиниГОСТ\n\n")
        f.write(f"**Ключ:** `0x{key:08X}`\n\n")
        f.write(f"**Размер:** 65536 записей (все возможные 16-битные блоки)\n\n")
        f.write(f"**Использование:**\n")
        f.write(f"```c\n")
        f.write(f"uint16_t plaintext = 0x1234;\n")
        f.write(f"uint16_t ciphertext = LOOKUP_TABLE[plaintext];  // Мгновенное шифрование!\n")
        f.write(f"```\n\n")
        
        f.write("## Таблица шифрования\n\n")
        f.write("| Блок | Зашифрованный | Блок | Зашифрованный | Блок | Зашифрованный | Блок | Зашифрованный |\n")
        f.write("|------|---------------|------|---------------|------|---------------|------|---------------|\n")
        
        # Записываем по 4 значения в строке таблицы
        for i in range(0, len(lookup_table), 4):
            row_values = []
            for j in range(4):
                if i + j < len(lookup_table):
                    block = i + j
                    encrypted = lookup_table[block]
                    row_values.append(f"0x{block:04X}")
                    row_values.append(f"0x{encrypted:04X}")
            f.write("| " + " | ".join(row_values) + " |\n")
        
        f.write("\n## Статистика\n\n")
        
        # Подсчитываем статистику
        unique_values = len(set(lookup_table))
        f.write(f"- Всего блоков: 65536\n")
        f.write(f"- Уникальных значений: {unique_values}\n")
        f.write(f"- Коллизий: {65536 - unique_values}\n")
        
        # Находим самые частые значения
        from collections import Counter
        counter = Counter(lookup_table)
        most_common = counter.most_common(5)
        
        f.write(f"\nСамые частые зашифрованные значения:\n")
        for value, count in most_common:
            f.write(f"- `0x{value:04X}`: встречается {count} раз\n")
    
    size_kb = output_path.stat().st_size / 1024
    print(f"Сохранено как Markdown: {output_path} ({size_kb:.1f} KB)")


def save_as_python(lookup_table: list[int], key: int, output_path: Path) -> None:
    """
    Сохраняет таблицу как Python модуль для быстрого импорта.
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        f.write(f'''"""
Автоматически сгенерированная lookup-таблица для МиниГОСТ
Ключ: 0x{key:08X}
"""

LOOKUP_TABLE_KEY = 0x{key:08X}

# Таблица шифрования: LOOKUP_TABLE[plaintext] = ciphertext
LOOKUP_TABLE = [
''')
        
        # Записываем таблицу по 8 значений в строке
        for i in range(0, len(lookup_table), 8):
            values = lookup_table[i:i+8]
            line = "    " + ", ".join(f"0x{v:04X}" for v in values) + ","
            f.write(line + "\n")
        
        f.write(''']

def encrypt_lookup(plaintext: int) -> int:
    """Шифрование через lookup - O(1)"""
    if not 0 <= plaintext <= 0xFFFF:
        raise ValueError("plaintext must be 0..0xFFFF")
    return LOOKUP_TABLE[plaintext]


if __name__ == "__main__":
    # Тест
    test_block = 0x1234
    encrypted = encrypt_lookup(test_block)
    print(f"Тест: 0x{test_block:04X} -> 0x{encrypted:04X}")
''')
    
    size_kb = output_path.stat().st_size / 1024
    print(f"Сохранено как Python модуль: {output_path} ({size_kb:.1f} KB)")


def parse_key(value: str) -> int:
    """Парсит 32-битный ключ из строки."""
    try:
        parsed = int(value, 0)
        if not 0 <= parsed <= 0xFFFFFFFF:
            raise argparse.ArgumentTypeError("key must be 0..0xFFFFFFFF")
        return parsed
    except ValueError:
        raise argparse.ArgumentTypeError(f"Invalid key: {value}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Генератор lookup-таблиц для МиниГОСТ",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры:
  # Генерация всех форматов для ключа
  %(prog)s --key 0xA1B2C3D4
  
  # Только бинарный формат (самый компактный)
  %(prog)s --key 0xA1B2C3D4 --format binary
  
  # C header для встраивания в код
  %(prog)s --key 0x12345678 --format c-header
  
  # Markdown для документации
  %(prog)s --key 0x00000000 --format markdown
        """
    )
    
    parser.add_argument(
        "--key",
        type=parse_key,
        required=True,
        help="32-битный ключ шифрования (например: 0xA1B2C3D4)"
    )
    
    parser.add_argument(
        "--format",
        choices=["all", "binary", "c-header", "markdown", "python"],
        default="all",
        help="Формат выходного файла (по умолчанию: all)"
    )
    
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "lookup_tables",
        help="Директория для сохранения таблиц (по умолчанию: ./lookup_tables)"
    )
    
    parser.add_argument(
        "--prefix",
        type=str,
        default="",
        help="Префикс для имён файлов (по умолчанию: нет)"
    )
    
    args = parser.parse_args()
    
    # Генерируем таблицу
    lookup_table = generate_lookup_table(args.key)
    
    # Формируем базовое имя файла
    key_hex = f"{args.key:08X}"
    
    # Если указан префикс, добавляем его И ключ
    # Если не указан, используем только ключ
    if args.prefix:
        # Префикс + ключ + lookup
        base_name = f"{args.prefix}{key_hex}_lookup"
    else:
        # По умолчанию: ключ + lookup
        base_name = f"{key_hex}_lookup"
    
    print()
    
    # Сохраняем в нужных форматах
    formats_to_save = {
        "binary": (save_as_binary, ".bin"),
        "c-header": (save_as_c_header, ".h"),
        "markdown": (save_as_markdown, ".md"),
        "python": (save_as_python, ".py"),
    }
    
    if args.format == "all":
        formats = formats_to_save.keys()
    else:
        formats = [args.format]
    
    for fmt in formats:
        save_func, ext = formats_to_save[fmt]
        output_path = args.output_dir / f"{base_name}{ext}"
        save_func(lookup_table, args.key, output_path)
    
    print()
    print("=" * 70)
    print("✓ Генерация завершена успешно!")
    print("=" * 70)
    print()
    print("Использование lookup-таблицы в C:")
    print(f"  #include \"lookup_tables/{base_name}.h\"")
    print(f"  uint16_t encrypted = minigost_encrypt_lookup(plaintext);")
    print()
    print("Использование в Python:")
    print(f"  from lookup_tables.{base_name[:-3] if base_name.endswith('.py') else base_name} import encrypt_lookup")
    print(f"  encrypted = encrypt_lookup(plaintext)")
    print()
    
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
