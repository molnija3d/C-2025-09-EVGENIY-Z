#!/bin/bash
if [ $# -eq 0 ]; then
    echo "Ошибка: не указан PID"
    echo "Использование: $0 <PID> [дополнительные_параметры]"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "Требуются права root. Запускайте через sudo"
    exit 1
fi

# Настройка ftrace
MYPID=$1
TRACE_DIR="/sys/kernel/tracing"

cd $TRACE_DIR
echo "Настройка ftrace..."

# Сброс конфигурации
printf 0 > tracing_on
echo > trace

printf "function" > current_tracer

printf "__x64_sys_*" > set_ftrace_filter


printf "%d" $MYPID > set_ftrace_pid


printf "Using PID: %d\n" $MYPID

echo "Запуск трассировки..."
echo 1 > tracing_on
read -p "Продолжите программу во втором терминале, после завершения работы нажмите ENTER"

echo "Остановка трассировки..."

echo 0 > tracing_on

echo "Результаты трассировки:"
echo "======================="

cat trace | head -500

cat trace > /tmp/ftrace_resultrs.txt
echo "Результаты сохранены в /tmp/ftrace_results"

echo > set_ftrace_pid
