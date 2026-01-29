#!/bin/bash

# Настройка ftrace
TRACE_DIR="/sys/kernel/debug/tracing"

echo "Настройка ftrace..."

# Сброс конфигурации
echo 0 > $TRACE_DIR/tracing_on
echo > $TRACE_DIR/trace

# Настройка событий системных вызовов
echo > $TRACE_DIR/set_event
echo 'syscalls:sys_enter_*' > $TRACE_DIR/set_event

# Включение трассировки стека
echo stacktrace > $TRACE_DIR/trace_options

# Ограничение буфера
echo 2048 > $TRACE_DIR/buffer_size_kb

echo "Запуск трассировки..."
echo 1 > $TRACE_DIR/tracing_on

echo "Выполнение программы..."
./syscall_demo

echo "Остановка трассировки..."
echo 0 > $TRACE_DIR/tracing_on

echo "Результаты трассировки:"
echo "========================"
cat $TRACE_DIR/trace | head -200

# Сохранение результатов
cat $TRACE_DIR/trace > ftrace_results.txt
echo "Результаты сохранены в ftrace_results.txt"