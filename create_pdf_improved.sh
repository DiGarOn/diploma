#!/bin/bash
# Улучшенное создание PDF с правильным форматированием

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║     Создание PDF с улучшенным форматированием             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Добавляем путь к LaTeX
export PATH="/Library/TeX/texbin:$PATH"

# Проверки
if [ ! -f "IMPLEMENTATION_SHORT.md" ]; then
    echo "❌ Файл IMPLEMENTATION_SHORT.md не найден"
    exit 1
fi

if ! command -v pandoc >/dev/null 2>&1; then
    echo "❌ pandoc не установлен"
    exit 1
fi

if ! command -v xelatex >/dev/null 2>&1; then
    echo "❌ LaTeX не установлен"
    exit 1
fi

echo "✓ Все инструменты готовы"
echo ""

# Создаём временный файл с настройками LaTeX
cat > /tmp/preamble.tex << 'EOF'
% Настройки для правильного отображения
\usepackage{geometry}
\geometry{
    a4paper,
    left=2cm,
    right=2cm,
    top=2.5cm,
    bottom=2.5cm
}

% Пакеты для кода
\usepackage{fancyvrb}
\usepackage{listings}
\usepackage{xcolor}

% Настройки для длинных строк кода
\lstset{
    basicstyle=\ttfamily\small,
    breaklines=true,
    breakatwhitespace=true,
    postbreak=\mbox{\textcolor{gray}{$\hookrightarrow$}\space},
    columns=flexible,
    keepspaces=true,
    showstringspaces=false,
    frame=single,
    framesep=3mm,
    xleftmargin=3mm,
    xrightmargin=3mm,
    backgroundcolor=\color{gray!10}
}

% Настройки для Verbatim (если используется)
\fvset{
    fontsize=\small,
    breaklines=true,
    breakanywhere=true,
    breaksymbol={\tiny$\hookrightarrow$},
    breakbefore=.,
    frame=single,
    framesep=3mm,
    xleftmargin=3mm,
    xrightmargin=3mm
}

% Для правильного отображения формул
\usepackage{amsmath}
\usepackage{amssymb}

% Переносы в формулах
\allowdisplaybreaks

% Настройки для таблиц
\usepackage{longtable}
\usepackage{booktabs}
\usepackage{array}

% Предотвращение переполнения строк
\setlength{\emergencystretch}{3em}
\tolerance=1000
\hbadness=10000

% Улучшенные переносы
\usepackage[hyphens]{url}
\usepackage{breakurl}

% Настройки шрифтов
\usepackage{fontspec}
\setmonofont[Scale=0.85]{Courier New}

% Для русского языка
\usepackage{polyglossia}
\setdefaultlanguage{russian}
\setotherlanguage{english}

% Улучшенные заголовки
\usepackage{titlesec}
\titleformat{\section}{\large\bfseries}{\thesection}{1em}{}
\titleformat{\subsection}{\normalsize\bfseries}{\thesubsection}{1em}{}

% Уменьшаем отступы
\setlength{\parskip}{6pt}
\setlength{\parindent}{0pt}

% Настройки для гиперссылок
\usepackage{hyperref}
\hypersetup{
    colorlinks=true,
    linkcolor=blue,
    urlcolor=blue,
    citecolor=blue
}
EOF

echo "════════════════════════════════════════════════════════════"
echo "Создание PDF с оптимизированными настройками..."
echo "════════════════════════════════════════════════════════════"
echo ""

# Создаём PDF
pandoc IMPLEMENTATION_SHORT.md \
    -o IMPLEMENTATION_SHORT.pdf \
    --pdf-engine=xelatex \
    --include-in-header=/tmp/preamble.tex \
    --toc \
    --toc-depth=3 \
    --number-sections \
    -V documentclass=article \
    -V papersize=a4 \
    -V fontsize=10pt \
    -V lang=ru-RU \
    --wrap=preserve \
    2>&1 | tee /tmp/pandoc_output.log

# Проверяем результат
if [ -f "IMPLEMENTATION_SHORT.pdf" ]; then
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "✓✓✓ PDF создан с улучшенным форматированием! ✓✓✓"
    echo "════════════════════════════════════════════════════════════"
    echo ""
    
    SIZE=$(du -h "IMPLEMENTATION_SHORT.pdf" | cut -f1)
    PAGES=$(mdls -name kMDItemNumberOfPages -raw "IMPLEMENTATION_SHORT.pdf" 2>/dev/null || echo "?")
    
    echo "📄 Файл: IMPLEMENTATION_SHORT.pdf"
    echo "📊 Размер: $SIZE"
    echo "📖 Страниц: $PAGES"
    echo ""
    echo "Улучшения:"
    echo "  ✓ Автоматические переносы длинных строк"
    echo "  ✓ Уменьшенный шрифт для кода (9pt)"
    echo "  ✓ Оптимизированные поля (2см)"
    echo "  ✓ Нумерация разделов"
    echo "  ✓ Оглавление"
    echo ""
    
    # Открываем
    open IMPLEMENTATION_SHORT.pdf
    
    echo "✓ PDF открыт"
else
    echo ""
    echo "❌ Ошибка создания PDF"
    cat /tmp/pandoc_output.log
    exit 1
fi

# Очистка
rm -f /tmp/preamble.tex /tmp/pandoc_output.log
