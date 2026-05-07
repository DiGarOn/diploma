# Makefile для проекта МиниГОСТ
# Упрощенная реализация алгоритма блочного шифрования

# ============================================================================
# Конфигурация компилятора
# ============================================================================

CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11 -march=native
INCLUDES = -Iinclude

# -O3: максимальная оптимизация
# -march=native: использовать все инструкции процессора для максимальной скорости
# -std=c11: стандарт C11
# -Wall -Wextra: все предупреждения

# Определяем расширение для shared library в зависимости от ОС
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    SHARED_EXT = dylib
    SHARED_FLAGS = -dynamiclib
else
    SHARED_EXT = so
    SHARED_FLAGS = -shared
endif

# ============================================================================
# Директории
# ============================================================================

SRC_DIR = src
INCLUDE_DIR = include
EXAMPLES_DIR = examples
BENCHMARK_DIR = benchmarks
BUILD_DIR = build
LIB_DIR = lib
TOOLS_DIR = tools

# ============================================================================
# Файлы
# ============================================================================

CORE_SRC = $(SRC_DIR)/minigost.c
CORE_HEADER = $(INCLUDE_DIR)/minigost.h
STATIC_LIB = $(LIB_DIR)/libminigost.a
SHARED_LIB = $(LIB_DIR)/libminigost.$(SHARED_EXT)

# Объектные файлы
CORE_OBJ = $(BUILD_DIR)/minigost.o
CORE_OBJ_SHARED = $(BUILD_DIR)/minigost_shared.o

# Исполняемые файлы
TEST_EXEC = $(BUILD_DIR)/minigost_test
EXAMPLE_EXEC = $(BUILD_DIR)/example
BENCHMARK_EXEC = $(BUILD_DIR)/benchmark
COMPARE_RUNNER = $(BUILD_DIR)/minigost_c_runner

# ============================================================================
# Основные цели
# ============================================================================

.PHONY: all clean test benchmark example python compare dirs help

all: dirs $(STATIC_LIB) $(TEST_EXEC)

# Создание необходимых директорий
dirs:
	@mkdir -p $(BUILD_DIR) $(LIB_DIR)

# ============================================================================
# Библиотеки
# ============================================================================

# Статическая библиотека
$(STATIC_LIB): $(CORE_OBJ) | dirs
	ar rcs $@ $^
	@echo "✓ Собрана статическая библиотека: $@"

$(CORE_OBJ): $(CORE_SRC) $(CORE_HEADER) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -DMINIGOST_NO_MAIN -c $< -o $@

# Shared library для Python
$(SHARED_LIB): $(CORE_OBJ_SHARED) | dirs
	$(CC) $(CFLAGS) $(SHARED_FLAGS) -o $@ $^
	@echo "✓ Собрана shared библиотека: $@"

$(CORE_OBJ_SHARED): $(CORE_SRC) $(CORE_HEADER) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -DMINIGOST_NO_MAIN -c $< -o $@

# ============================================================================
# Исполняемые файлы
# ============================================================================

# Тестовый исполняемый файл (встроенные тесты)
$(TEST_EXEC): $(CORE_SRC) $(CORE_HEADER) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(CORE_SRC)
	@echo "✓ Собран исполняемый файл с тестами: $@"

# Пример использования библиотеки
$(EXAMPLE_EXEC): $(EXAMPLES_DIR)/example_usage.c $(STATIC_LIB) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -L$(LIB_DIR) -lminigost
	@echo "✓ Собран пример использования: $@"

# Бенчмарк производительности
$(BENCHMARK_EXEC): $(BENCHMARK_DIR)/minigost_benchmark.c $(STATIC_LIB) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< -L$(LIB_DIR) -lminigost
	@echo "✓ Собран бенчмарк: $@"

# CLI-раннер для сравнения C- и Python-реализаций
$(COMPARE_RUNNER): $(TOOLS_DIR)/minigost_c_runner.c $(CORE_SRC) $(CORE_HEADER) | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -DMINIGOST_NO_MAIN -o $@ $(TOOLS_DIR)/minigost_c_runner.c $(CORE_SRC)
	@echo "✓ Собран CLI-раннер для сравнения: $@"

# ============================================================================
# Запуск и тестирование
# ============================================================================

# Запуск встроенных тестов
test: $(TEST_EXEC)
	@echo "════════════════════════════════════════════════════════"
	@echo "Запуск встроенных тестов МиниГОСТ..."
	@echo "════════════════════════════════════════════════════════"
	@$(TEST_EXEC)

# Запуск примера использования
example: $(EXAMPLE_EXEC)
	@echo "════════════════════════════════════════════════════════"
	@echo "Запуск примера использования библиотеки..."
	@echo "════════════════════════════════════════════════════════"
	@$(EXAMPLE_EXEC)

# Запуск бенчмарка
benchmark: $(BENCHMARK_EXEC)
	@echo "════════════════════════════════════════════════════════"
	@echo "Запуск бенчмарка производительности..."
	@echo "════════════════════════════════════════════════════════"
	@$(BENCHMARK_EXEC)

# Python обертка
python: $(SHARED_LIB)
	@echo "════════════════════════════════════════════════════════"
	@echo "Запуск Python обертки..."
	@echo "════════════════════════════════════════════════════════"
	@cd $(SRC_DIR) && DYLD_LIBRARY_PATH=../$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=../$(LIB_DIR):$$LD_LIBRARY_PATH python3 minigost_python.py

# Сравнение pure-Python и C реализаций на всех 16-битных блоках
compare: $(COMPARE_RUNNER)
	@echo "════════════════════════════════════════════════════════"
	@echo "Сравнение Python и C реализаций МиниГОСТ..."
	@echo "════════════════════════════════════════════════════════"
	@python3 $(TOOLS_DIR)/compare_minigost_impls.py --py-key 0xA1B2C3D4

# Быстрый запуск: компиляция + тесты
run: test

# ============================================================================
# Очистка
# ============================================================================

clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)
	@echo "✓ Удалены все скомпилированные файлы и директории"

# Глубокая очистка (включая Python кэши)
distclean: clean
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
	@echo "✓ Выполнена глубокая очистка"

# ============================================================================
# Справка
# ============================================================================

help:
	@echo "════════════════════════════════════════════════════════"
	@echo "  Makefile для проекта МиниГОСТ"
	@echo "════════════════════════════════════════════════════════"
	@echo ""
	@echo "Основные команды:"
	@echo "  make               - Собрать библиотеку и тесты"
	@echo "  make test          - Собрать и запустить встроенные тесты"
	@echo "  make run           - То же, что и make test"
	@echo "  make example       - Собрать и запустить пример использования"
	@echo "  make benchmark     - Собрать и запустить бенчмарк"
	@echo "  make python        - Собрать shared library и запустить Python обертку"
	@echo "  make compare       - Собрать раннер и сравнить Python/C реализации"
	@echo ""
	@echo "Сборка компонентов:"
	@echo "  make $(STATIC_LIB) - Собрать только статическую библиотеку"
	@echo "  make $(SHARED_LIB) - Собрать только shared библиотеку"
	@echo "  make $(COMPARE_RUNNER) - Собрать раннер для compare-скрипта"
	@echo ""
	@echo "Очистка:"
	@echo "  make clean         - Удалить скомпилированные файлы"
	@echo "  make distclean     - Полная очистка (включая Python кэши)"
	@echo ""
	@echo "Справка:"
	@echo "  make help          - Показать эту справку"
	@echo "════════════════════════════════════════════════════════"

# ============================================================================
# Информация о проекте
# ============================================================================

info:
	@echo "════════════════════════════════════════════════════════"
	@echo "  Информация о проекте МиниГОСТ"
	@echo "════════════════════════════════════════════════════════"
	@echo "Компилятор:       $(CC)"
	@echo "Флаги компиляции: $(CFLAGS)"
	@echo "Включения:        $(INCLUDES)"
	@echo "ОС:               $(UNAME_S)"
	@echo "Расширение .so:   $(SHARED_EXT)"
	@echo "════════════════════════════════════════════════════════"
	@echo "Исходники:"
	@echo "  - $(CORE_SRC)"
	@echo "  - $(CORE_HEADER)"
	@echo "════════════════════════════════════════════════════════"
	@echo "Библиотеки:"
	@echo "  - $(STATIC_LIB)"
	@echo "  - $(SHARED_LIB)"
	@echo "════════════════════════════════════════════════════════"
