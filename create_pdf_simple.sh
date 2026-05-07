#!/bin/bash
# Скрипт для создания PDF из IMPLEMENTATION_SHORT.md
# Автоматически устанавливает все необходимые зависимости

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        Создание PDF из IMPLEMENTATION_SHORT.md            ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Добавляем путь к LaTeX
export PATH="/Library/TeX/texbin:$PATH"

# Проверяем наличие исходного файла
if [ ! -f "IMPLEMENTATION_SHORT.md" ]; then
    echo "❌ Файл IMPLEMENTATION_SHORT.md не найден"
    exit 1
fi

# Проверяем pandoc
if ! command -v pandoc >/dev/null 2>&1; then
    echo "❌ pandoc не установлен. Установите: brew install pandoc"
    exit 1
fi

# Проверяем LaTeX
if ! command -v xelatex >/dev/null 2>&1; then
    echo "❌ LaTeX не установлен"
    echo "Установите BasicTeX: brew install --cask basictex"
    echo "Затем выполните: export PATH=\"/Library/TeX/texbin:\$PATH\""
    exit 1
fi

echo "✓ pandoc установлен"
echo "✓ LaTeX установлен"
echo ""

# Устанавливаем необходимые LaTeX пакеты
echo "📦 Проверка и установка необходимых LaTeX пакетов..."
echo "    (это может занять несколько минут)"
echo ""

# Список необходимых пакетов
PACKAGES=(
    "framed"
    "fancyvrb"
    "mdwtools"
    "soul"
    "xcolor"
    "caption"
    "listings"
    "fvextra"
    "upquote"
    "lineno"
    "booktabs"
    "footnotehyper"
)

for pkg in "${PACKAGES[@]}"; do
    echo "  Установка $pkg..."
    sudo tlmgr install "$pkg" 2>/dev/null || echo "    (уже установлен или пропущен)"
done

echo ""
echo "✓ Пакеты установлены"
echo ""

echo "════════════════════════════════════════════════════════════"
echo "Создание PDF..."
echo "════════════════════════════════════════════════════════════"
echo ""

# Создаём PDF с помощью pandoc (упрощённая версия без подсветки синтаксиса)
pandoc IMPLEMENTATION_SHORT.md \
    -o IMPLEMENTATION_SHORT.pdf \
    --pdf-engine=xelatex \
    -V geometry:margin=2.5cm \
    -V fontsize=11pt \
    -V documentclass=article \
    -V lang=ru \
    -V mainfont="Times New Roman" \
    --toc \
    --toc-depth=3 \
    --listings \
    2>&1 | tee /tmp/pandoc_output.log

# Проверяем успешность
if [ -f "IMPLEMENTATION_SHORT.pdf" ]; then
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "✓✓✓ PDF успешно создан! ✓✓✓"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    echo "📄 Файл: IMPLEMENTATION_SHORT.pdf"
    
    # Показываем размер файла
    SIZE=$(du -h "IMPLEMENTATION_SHORT.pdf" | cut -f1)
    echo "📊 Размер: $SIZE"
    echo ""
    
    # Открываем PDF
    echo "Открываю PDF..."
    open IMPLEMENTATION_SHORT.pdf
else
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "❌ Ошибка при создании PDF"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    echo "Лог ошибок:"
    cat /tmp/pandoc_output.log
    echo ""
    echo "Попробуйте альтернативный метод:"
    echo "  1. Откройте IMPLEMENTATION_SHORT.html в браузере"
    echo "  2. Нажмите Cmd+P"
    echo "  3. Выберите 'Сохранить как PDF'"
    echo ""
    exit 1
fi
