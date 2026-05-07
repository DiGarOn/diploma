#!/bin/bash
#
# Быстрая генерация lookup-таблицы для МиниГОСТ
# с рекомендованным ключом
#

# Рекомендованный ключ для тестирования и демонстрации
RECOMMENDED_KEY="0xDEADBEEF"

echo "════════════════════════════════════════════════════════════════"
echo "  Генерация lookup-таблицы МиниГОСТ"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Рекомендованный ключ: $RECOMMENDED_KEY"
echo "  - Легко запоминается"
echo "  - Классический 'магический' ключ"
echo "  - Хорошо виден в hex дампах"
echo ""

# Если передан аргумент, используем его как ключ
KEY="${1:-$RECOMMENDED_KEY}"

echo "Используется ключ: $KEY"
echo ""

# Генерируем все форматы
python3 tools/generate_lookup_table.py --key "$KEY"

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  Готово! Файлы сохранены в lookup_tables/"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Теперь можно использовать:"
echo ""
echo "В C:"
KEY_HEX=$(printf "%08X" $KEY 2>/dev/null || echo "DEADBEEF")
echo "  #include \"lookup_tables/${KEY_HEX}_lookup.h\""
echo "  uint16_t encrypted = minigost_encrypt_lookup(plaintext);"
echo ""
echo "В Python:"
echo "  from lookup_tables.${KEY_HEX}_lookup import encrypt_lookup"
echo "  encrypted = encrypt_lookup(plaintext)"
echo ""
