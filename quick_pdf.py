#!/usr/bin/env python3
"""
Быстрое создание PDF из Markdown без LaTeX
Использует WeasyPrint для генерации PDF
"""

import subprocess
import sys
import os

def install_package(package):
    """Устанавливает Python пакет"""
    subprocess.check_call([sys.executable, "-m", "pip", "install", "-q", package])

def main():
    print("╔════════════════════════════════════════════════════════════╗")
    print("║     Быстрое создание PDF (без LaTeX, через Python)       ║")
    print("╚════════════════════════════════════════════════════════════╝")
    print()
    
    # Проверяем и устанавливаем необходимые пакеты
    required = ['markdown2', 'weasyprint', 'pygments']
    
    for package in required:
        try:
            __import__(package if package != 'markdown2' else 'markdown2')
            print(f"✓ {package} уже установлен")
        except ImportError:
            print(f"📦 Установка {package}...")
            try:
                install_package(package)
                print(f"✓ {package} установлен")
            except Exception as e:
                print(f"❌ Ошибка установки {package}: {e}")
                return False
    
    print()
    print("════════════════════════════════════════════════════════════")
    print("Создание PDF...")
    print("════════════════════════════════════════════════════════════")
    print()
    
    try:
        import markdown2
        from weasyprint import HTML, CSS
        from pygments.formatters import HtmlFormatter
        
        # Читаем markdown
        with open('IMPLEMENTATION_SHORT.md', 'r', encoding='utf-8') as f:
            md_content = f.read()
        
        # Конвертируем в HTML
        html_body = markdown2.markdown(
            md_content,
            extras=['fenced-code-blocks', 'tables', 'header-ids', 'toc']
        )
        
        # CSS стили
        css = '''
            @page {
                size: A4;
                margin: 2.5cm;
                @top-right {
                    content: "МиниГОСТ - Реализация | Страница " counter(page);
                    font-size: 9pt;
                    color: #666;
                }
            }
            
            body {
                font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Arial, sans-serif;
                line-height: 1.6;
                color: #333;
                font-size: 11pt;
            }
            
            h1 {
                color: #2c3e50;
                border-bottom: 3px solid #3498db;
                padding-bottom: 10px;
                margin-top: 30px;
                page-break-after: avoid;
            }
            
            h2 {
                color: #34495e;
                border-bottom: 2px solid #bdc3c7;
                padding-bottom: 8px;
                margin-top: 25px;
                page-break-after: avoid;
            }
            
            h3 {
                color: #7f8c8d;
                margin-top: 20px;
                page-break-after: avoid;
            }
            
            code {
                background-color: #f4f4f4;
                padding: 2px 6px;
                border-radius: 3px;
                font-family: "SF Mono", Monaco, Consolas, monospace;
                font-size: 0.9em;
                color: #e74c3c;
            }
            
            pre {
                background-color: #f6f8fa;
                border: 1px solid #d1d5da;
                border-radius: 6px;
                padding: 16px;
                overflow-x: auto;
                page-break-inside: avoid;
                margin: 15px 0;
            }
            
            pre code {
                background-color: transparent;
                padding: 0;
                color: #24292e;
                font-size: 9pt;
            }
            
            table {
                border-collapse: collapse;
                width: 100%;
                margin: 15px 0;
                page-break-inside: avoid;
            }
            
            th, td {
                border: 1px solid #ddd;
                padding: 8px;
                text-align: left;
            }
            
            th {
                background-color: #f2f2f2;
                font-weight: bold;
            }
            
            p {
                margin: 10px 0;
            }
            
            strong {
                color: #2c3e50;
                font-weight: 600;
            }
        '''
        
        # Создаём полный HTML
        full_html = f'''<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>МиниГОСТ - Реализация</title>
</head>
<body>
    <h1 style="text-align: center; border-bottom: none;">МиниГОСТ</h1>
    <p style="text-align: center; color: #7f8c8d;">Упрощённая учебная реализация блочного шифрования</p>
    <hr style="margin: 30px 0;">
    {html_body}
</body>
</html>'''
        
        # Создаём PDF
        HTML(string=full_html).write_pdf(
            'IMPLEMENTATION_SHORT.pdf',
            stylesheets=[CSS(string=css)]
        )
        
        # Проверяем что файл создан
        if os.path.exists('IMPLEMENTATION_SHORT.pdf'):
            size = os.path.getsize('IMPLEMENTATION_SHORT.pdf')
            size_mb = size / (1024 * 1024)
            
            print()
            print("════════════════════════════════════════════════════════════")
            print("✓✓✓ PDF успешно создан! ✓✓✓")
            print("════════════════════════════════════════════════════════════")
            print()
            print(f"📄 Файл: IMPLEMENTATION_SHORT.pdf")
            print(f"📊 Размер: {size_mb:.2f} MB")
            print()
            
            # Открываем PDF
            subprocess.run(['open', 'IMPLEMENTATION_SHORT.pdf'])
            
            return True
        else:
            print("❌ PDF файл не был создан")
            return False
            
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == '__main__':
    success = main()
    sys.exit(0 if success else 1)
