#!/bin/bash
# Финальная версия: надёжное создание красивого PDF

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║          Создание красивого PDF (финальная версия)        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

export PATH="/Library/TeX/texbin:$PATH"

# Проверки
if [ ! -f "IMPLEMENTATION_SHORT.md" ]; then
    echo "❌ Файл не найден"
    exit 1
fi

echo "✓ Начинаем создание PDF..."
echo ""

# Создаём упрощённый LaTeX header
cat > /tmp/header.tex << 'EOF'
\usepackage{geometry}
\geometry{a4paper, margin=2cm}

% Для кода
\usepackage{fvextra}
\DefineVerbatimEnvironment{Highlighting}{Verbatim}{
    fontsize=\small,
    breaklines,
    breaksymbol=\space,
    commandchars=\\\{\}
}

% Математика
\usepackage{amsmath}
\usepackage{amssymb}

% Русский язык
\usepackage{polyglossia}
\setdefaultlanguage{russian}

% Шрифты
\usepackage{fontspec}
\setmainfont{Times New Roman}
\newfontfamily\cyrillicfont{Times New Roman}
\setmonofont[Scale=0.8]{Menlo}
\newfontfamily\cyrillicfonttt[Scale=0.8]{Menlo}
\defaultfontfeatures{Ligatures=TeX}

% Таблицы
\usepackage{longtable}
\usepackage{booktabs}

% Гиперссылки
\usepackage{hyperref}
\hypersetup{
    colorlinks=true,
    linkcolor=blue,
    urlcolor=blue
}

% Переносы
\setlength{\emergencystretch}{3em}
\tolerance=9999
EOF

# Создаём PDF с pandoc
pandoc IMPLEMENTATION_SHORT.md \
    -o IMPLEMENTATION_SHORT.pdf \
    --pdf-engine=xelatex \
    --include-in-header=/tmp/header.tex \
    --toc \
    --toc-depth=2 \
    --number-sections \
    -V fontsize=10pt \
    -V papersize=a4 \
    -V lang=ru-RU \
    2>&1

if [ -f "IMPLEMENTATION_SHORT.pdf" ]; then
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "✓✓✓ PDF успешно создан! ✓✓✓"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    
    SIZE=$(du -h "IMPLEMENTATION_SHORT.pdf" | cut -f1)
    echo "📄 Файл: IMPLEMENTATION_SHORT.pdf"
    echo "📊 Размер: $SIZE"
    echo ""
    echo "Настройки:"
    echo "  ✓ Поля: 2см со всех сторон"
    echo "  ✓ Код: автоматические переносы строк"
    echo "  ✓ Шрифт кода: Monaco, размер 0.8"
    echo "  ✓ Оглавление с нумерацией"
    echo "  ✓ Формулы: LaTeX математика"
    echo ""
    
    open IMPLEMENTATION_SHORT.pdf
else
    echo "❌ Ошибка создания PDF"
    exit 1
fi

rm -f /tmp/header.tex
