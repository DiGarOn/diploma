# Рекомендованный ключ для МиниГОСТ

## 🔑 Рекомендованный ключ

```
0xDEADBEEFCAFEBABE
```

### Почему именно этот ключ?

✅ **Легко запоминается** — содержит известные hex-слова:
- `DEAD` — мёртвый
- `BEEF` — говядина  
- `CAFE` — кафе
- `BABE` — красотка

✅ **Классический в Computer Science** — часто используется в:
- Отладке
- Примерах кода
- Hex дампах памяти
- Тестировании

✅ **Хорошо виден** — легко найти в логах и дампах

✅ **Полноценный 64-битный ключ** — использует все байты

---

## 🚀 Быстрый старт

### Самый простой способ:

```bash
./generate_lookup.sh
```

Это создаст lookup-таблицы для рекомендованного ключа `0xDEADBEEFCAFEBABE`.

### С другим ключом:

```bash
./generate_lookup.sh 0xYOURKEY12345678
```

---

## 📦 Созданные файлы

После запуска `./generate_lookup.sh` будут созданы:

```
lookup_tables/
├── DEADBEEFCAFEBABE_lookup.bin    # Бинарный (128 KB)
├── DEADBEEFCAFEBABE_lookup.h      # C header (545 KB)
├── DEADBEEFCAFEBABE_lookup.md     # Markdown (1.2 MB)
└── DEADBEEFCAFEBABE_lookup.py     # Python (545 KB)
```

**Обратите внимание:** Ключ теперь **в начале** имени файла, а не в середине!

---

## 💻 Использование

### В C:

```c
#include "lookup_tables/DEADBEEFCAFEBABE_lookup.h"

// Простое шифрование
uint16_t plaintext = 0x1234;
uint16_t encrypted = minigost_encrypt_lookup(plaintext);
// Результат: 0x6619

// Шифрование массива
void encrypt_data(const uint8_t *input, uint8_t *output, size_t len) {
    for (size_t i = 0; i < len; i += 2) {
        uint16_t block = (input[i] << 8) | input[i+1];
        uint16_t encrypted = minigost_encrypt_lookup(block);
        output[i] = (encrypted >> 8) & 0xFF;
        output[i+1] = encrypted & 0xFF;
    }
}
```

### В Python:

```python
from lookup_tables.DEADBEEFCAFEBABE_lookup import encrypt_lookup

# Простое шифрование
plaintext = 0x1234
encrypted = encrypt_lookup(plaintext)
# Результат: 0x6619

# Проверка
print(f"0x{plaintext:04X} -> 0x{encrypted:04X}")
```

---

## 📊 Примеры шифрования

Для ключа `0xDEADBEEFCAFEBABE`:

| Открытый текст | Зашифрованный |
|----------------|---------------|
| `0x0000` | `0x6342` |
| `0x1234` | `0x6619` |
| `0xDEAD` | `0xF20A` |
| `0xBEEF` | `0x37C2` |
| `0xCAFE` | `0xF4B5` |
| `0xBABE` | `0x3E77` |
| `0xFFFF` | `0x832F` |

---

## 🔧 Альтернативные ключи

Если вам нужен другой ключ:

### Для демонстрации и отладки:
```bash
# Простые паттерны
./generate_lookup.sh 0x0000000000000000  # Нулевой ключ
./generate_lookup.sh 0x1111111111111111  # Повторяющиеся единицы
./generate_lookup.sh 0xFFFFFFFFFFFFFFFF  # Все единицы
./generate_lookup.sh 0x0123456789ABCDEF  # Последовательность
```

### ASCII ключи:
```bash
# "MINIGOST" в ASCII hex
./generate_lookup.sh 0x4D494E49474F5354

# "PASSWORD" в ASCII hex
./generate_lookup.sh 0x50415353574F5244
```

### Случайные ключи:
```bash
# Генерация случайного ключа (Linux/macOS)
RANDOM_KEY=0x$(openssl rand -hex 8 | tr 'a-f' 'A-F')
./generate_lookup.sh $RANDOM_KEY
```

---

## 🎯 Формат имён файлов

Новый формат: **`КЛЮЧ_lookup.РАСШИРЕНИЕ`**

Примеры:
- `DEADBEEFCAFEBABE_lookup.h`
- `0123456789ABCDEF_lookup.py`
- `FFFFFFFFFFFFFFFF_lookup.bin`

Старый формат (больше не используется): ~~`lookup_key_XXXXXXXXXXXXXXXX`~~

---

## ✅ Проверка корректности

После генерации всегда проверяйте таблицу:

```bash
# Быстрый тест
python3 lookup_tables/DEADBEEFCAFEBABE_lookup.py

# Полная проверка
python3 -c "
from minigost import MiniGost
from lookup_tables.DEADBEEFCAFEBABE_lookup import encrypt_lookup, LOOKUP_TABLE_KEY

cipher = MiniGost()
test_blocks = [0x0000, 0x1234, 0xDEAD, 0xBEEF, 0xCAFE, 0xBABE, 0xFFFF]

all_ok = all(
    cipher.encrypt_block(b, LOOKUP_TABLE_KEY) == encrypt_lookup(b)
    for b in test_blocks
)

print('✅ Таблица корректна!' if all_ok else '❌ Ошибка!')
"
```

---

## 📝 Дополнительные команды

### Прямое использование генератора:

```bash
# Все форматы
python3 tools/generate_lookup_table.py --key 0xDEADBEEFCAFEBABE

# Только нужный формат
python3 tools/generate_lookup_table.py --key 0xDEADBEEFCAFEBABE --format c-header
python3 tools/generate_lookup_table.py --key 0xDEADBEEFCAFEBABE --format binary
python3 tools/generate_lookup_table.py --key 0xDEADBEEFCAFEBABE --format python
python3 tools/generate_lookup_table.py --key 0xDEADBEEFCAFEBABE --format markdown

# С префиксом (добавляется ПЕРЕД ключом)
python3 tools/generate_lookup_table.py --key 0xDEADBEEFCAFEBABE --prefix "test_"
# Создаст: test_DEADBEEFCAFEBABE_lookup.*
```

---

## 🎨 Hex-слова для ключей

Популярные hex-слова для создания запоминающихся ключей:

```
DEAD, BEEF, CAFE, BABE, FACE, FADE, FEED, DEED,
BEAD, DADA, DADE, EBBA, ACDC, ABBA, FADED, BEDDED,
C0DE, C0FFEE, DEFACED, FACADE, DECADE
```

Пример составного ключа:
```bash
./generate_lookup.sh 0xC0FFEEC0DEDECADE
```

---

## 📚 Документация

- **Полное руководство**: `docs/lookup_tables_guide.md`
- **Шпаргалка**: `docs/lookup_tables_cheatsheet.md`
- **Этот файл**: `docs/recommended_key.md`

---

## 🚀 Итого

**Для начала работы просто запустите:**

```bash
./generate_lookup.sh
```

Это создаст все необходимые файлы с рекомендованным ключом `0xDEADBEEFCAFEBABE`.

Имена файлов будут иметь формат: `DEADBEEFCAFEBABE_lookup.*`
