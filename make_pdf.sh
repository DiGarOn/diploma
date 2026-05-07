#!/bin/bash
# Версия с минимальными зависимостями (только базовые пакеты BasicTeX)

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Создание PDF (базовые пакеты, без sudo)             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

export PATH="/Library/TeX/texbin:$PATH"

if [ ! -f "IMPLEMENTATION_SHORT.md" ]; then
    echo "❌ Файл не найден"
    exit 1
fi

echo "✓ Создаём PDF с базовыми настройками..."
echo ""

# Минимальный header только с базовыми пакетами
cat > /tmp/minimal_header.tex << 'EOF'
% Геометрия страницы
\usepackage[a4paper, left=1.5cm, right=1.5cm, top=2cm, bottom=2cm]{geometry}

% Русский язык
\usepackage{polyglossia}
\setdefaultlanguage{russian}

% Шрифты
\usepackage{fontspec}
\setmainfont{Times New Roman}
\setmonofont[Scale=0.75]{Courier New}

% Математика
\usepackage{amsmath}

% Таблицы
\usepackage{longtable}
\usepackage{booktabs}

% Гиперссылки
\usepackage{hyperref}
\hypersetup{colorlinks=true, linkcolor=blue, urlcolor=blue}

% Улучшенные переносы
\tolerance=1000
\emergencystretch=3em

% Для кода - используем базовый verbatim
\usepackage{fancyvrb}
\RecustomVerbatimEnvironment{verbatim}{Verbatim}{fontsize=\footnotesize}

% Уменьшаем отступы
\setlength{\parskip}{4pt}
\setlength{\parindent}{0pt}
EOF

# Создаём PDF
pandoc IMPLEMENTATION_SHORT.md \
    -o IMPLEMENTATION_SHORT.pdf \
    --pdf-engine=xelatex \
    --include-in-header=/tmp/minimal_header.tex \
    --toc \
    --toc-depth=2 \
    -V fontsize=10pt \
    -V documentclass=article \
    2>&1

if [ -f "IMPLEMENTATION_SHORT.pdf" ]; then
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "✓✓✓ PDF создан! ✓✓✓"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    
    SIZE=$(du -h "IMPLEMENTATION_SHORT.pdf" | cut -f1)
    PAGES=$(mdls -name kMDItemNumberOfPages -raw "IMPLEMENTATION_SHORT.pdf" 2>/dev/null || echo "?")
    
    echo "📄 Файл: IMPLEMENTATION_SHORT.pdf"
    echo "📊 Размер: $SIZE"
    echo "📖 Страниц: $PAGES"
    echo ""
    echo "Настройки:"
    echo "  • Узкие поля (1.5см слева/справа)"
    echo "  • Уменьшенный шрифт кода"
    echo "  • Оглавление"
    echo "  • Times New Roman для текста"
    echo ""
    
    open IMPLEMENTATION_SHORT.pdf
    
    rm -f /tmp/minimal_header.tex
else
    echo "❌ Ошибка"
    rm -f /tmp/minimal_header.tex
    exit 1
fi
