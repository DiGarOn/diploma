#!/bin/bash
# Скрипт для создания PDF из IMPLEMENTATION_SHORT.md
# Автоматически устанавливает все необходимые зависимости

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        Создание PDF из IMPLEMENTATION_SHORT.md             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Функция для проверки команды
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Проверяем Homebrew
if ! command_exists brew; then
    echo "❌ Homebrew не найден. Установите его с https://brew.sh"
    exit 1
fi

# Проверяем и устанавливаем pandoc
if ! command_exists pandoc; then
    echo "📦 Установка pandoc..."
    brew install pandoc
else
    echo "✓ pandoc уже установлен"
fi

# Проверяем LaTeX (xelatex или pdflatex)
if ! command_exists xelatex && ! command_exists pdflatex; then
    echo ""
    echo "📦 Установка BasicTeX (это займёт несколько минут)..."
    echo "    BasicTeX - это компактная версия LaTeX (~100 MB)"
    echo ""
    
    # Устанавливаем BasicTeX через Homebrew
    brew install --cask basictex
    
    # Обновляем PATH для текущей сессии
    eval "$(/usr/libexec/path_helper)"
    export PATH="/Library/TeX/texbin:$PATH"
    
    echo ""
    echo "✓ BasicTeX установлен"
    echo ""
    echo "⚠️  Необходимо обновить PATH. Выполните:"
    echo "    export PATH=\"/Library/TeX/texbin:\$PATH\""
    echo "    или перезапустите терминал"
    echo ""
    
    # Обновляем tlmgr и устанавливаем необходимые пакеты
    echo "📦 Установка дополнительных LaTeX пакетов..."
    sudo tlmgr update --self 2>/dev/null || echo "Обновление tlmgr пропущено"
    sudo tlmgr install collection-fontsrecommended 2>/dev/null || echo "Пакеты уже установлены"
else
    echo "✓ LaTeX уже установлен"
fi

# Проверяем наличие исходного файла
if [ ! -f "IMPLEMENTATION_SHORT.md" ]; then
    echo "❌ Файл IMPLEMENTATION_SHORT.md не найден"
    exit 1
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo "Создание PDF..."
echo "════════════════════════════════════════════════════════════"
echo ""

# Добавляем путь к LaTeX
export PATH="/Library/TeX/texbin:$PATH"

# Создаём PDF с помощью pandoc
pandoc IMPLEMENTATION_SHORT.md \
    -o IMPLEMENTATION_SHORT.pdf \
    --pdf-engine=xelatex \
    -V geometry:margin=2.5cm \
    -V fontsize=11pt \
    -V documentclass=article \
    -V lang=ru-RU \
    -V mainfont="Times New Roman" \
    --toc \
    --toc-depth=3 \
    --highlight-style=tango \
    2>&1

if [ $? -eq 0 ]; then
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "✓✓✓ PDF успешно создан! ✓✓✓"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    echo "📄 Файл: IMPLEMENTATION_SHORT.pdf"
    
    # Показываем размер файла
    if [ -f "IMPLEMENTATION_SHORT.pdf" ]; then
        SIZE=$(du -h "IMPLEMENTATION_SHORT.pdf" | cut -f1)
        echo "📊 Размер: $SIZE"
    fi
    
    echo ""
    echo "Открыть PDF? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        open IMPLEMENTATION_SHORT.pdf
    fi
else
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "❌ Ошибка при создании PDF"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    echo "Попробуйте альтернативный метод:"
    echo "  1. Откройте IMPLEMENTATION_SHORT.html в браузере"
    echo "  2. Нажмите Cmd+P"
    echo "  3. Выберите 'Сохранить как PDF'"
    echo ""
    exit 1
fi
