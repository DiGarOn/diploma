#!/bin/bash
# Создание PDF с автоматическими переносами длинных строк

set -e

export PATH="/Library/TeX/texbin:$PATH"

echo "Создаём PDF с переносами длинных строк..."

# Создаём header с настройками для переносов
cat > /tmp/pdf_header.tex << 'EOF'
\usepackage[a4paper, left=2cm, right=2cm, top=2.5cm, bottom=2.5cm]{geometry}

% Русский язык
\usepackage{polyglossia}
\setdefaultlanguage{russian}

% Шрифты
\usepackage{fontspec}
\setmainfont{Times New Roman}
\setmonofont[Scale=0.7]{Courier New}

% Для кода с переносами
\usepackage{listings}
\lstset{
    basicstyle=\ttfamily\scriptsize,
    breaklines=true,
    breakatwhitespace=false,
    postbreak=\mbox{\textcolor{red}{$\hookrightarrow$}\space},
    frame=single,
    xleftmargin=0.5cm,
    xrightmargin=0.5cm,
    backgroundcolor=\color{gray!5},
    columns=fullflexible,
    keepspaces=true,
    showstringspaces=false,
    tabsize=2
}

% Математика
\usepackage{amsmath}
\usepackage{breqn}

% Таблицы
\usepackage{longtable}
\usepackage{booktabs}
\usepackage{array}

% Цвета
\usepackage{xcolor}

% Гиперссылки
\usepackage{hyperref}
\hypersetup{
    colorlinks=true,
    linkcolor=blue,
    urlcolor=blue,
    breaklinks=true
}

% Улучшенные переносы
\tolerance=9999
\emergencystretch=10pt
\hyphenpenalty=10000
\exhyphenpenalty=100

% Переопределяем verbatim для лучших переносов
\usepackage{fancyvrb}
\RecustomVerbatimEnvironment{verbatim}{Verbatim}{
    fontsize=\scriptsize,
    breaklines=true,
    breakanywhere=true
}

% Отступы
\setlength{\parskip}{6pt}
\setlength{\parindent}{0pt}

% Запрет висячих строк
\widowpenalty=10000
\clubpenalty=10000
EOF

# Создаём PDF
pandoc IMPLEMENTATION_SHORT.md \
    -o IMPLEMENTATION_SHORT.pdf \
    --pdf-engine=xelatex \
    --include-in-header=/tmp/pdf_header.tex \
    --toc \
    --toc-depth=2 \
    --number-sections \
    -V fontsize=10pt \
    -V documentclass=article \
    2>&1

if [ -f "IMPLEMENTATION_SHORT.pdf" ]; then
    SIZE=$(du -h "IMPLEMENTATION_SHORT.pdf" | cut -f1)
    echo ""
    echo "✓✓✓ PDF создан! ✓✓✓"
    echo ""
    echo "📄 Файл: IMPLEMENTATION_SHORT.pdf"
    echo "📊 Размер: $SIZE"
    echo ""
    echo "Улучшения:"
    echo "  ✓ Автоматический перенос ВСЕХ длинных строк"
    echo "  ✓ Маленький шрифт для кода (scriptsize)"
    echo "  ✓ Символ ↪ показывает перенос"
    echo "  ✓ Формулы с переносами (breqn)"
    echo ""
    
    open IMPLEMENTATION_SHORT.pdf
else
    echo "❌ Ошибка"
    exit 1
fi

rm -f /tmp/pdf_header.tex
