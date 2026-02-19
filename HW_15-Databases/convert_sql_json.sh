#!/bin/bash
# Конвертация SQL INSERT в JSON массив (для таблицы oscar)
# Использование: ./convert.sh oscar.sql > oscar.json

input_file="$1"

if [ -z "$input_file" ]; then
    echo "Usage: $0 <sql_file>" >&2
    exit 1
fi

# Извлекаем строки с INSERT
grep -i "^INSERT INTO oscar VALUES" "$input_file" | while IFS= read -r line; do
    # Удаляем "INSERT INTO oscar VALUES(" в начале и ");" в конце
    values=$(echo "$line" | sed -e "s/^INSERT INTO oscar VALUES(//" -e "s/);$//")
    
    # Разделяем по запятой (безопасно, т.к. внутри строк нет запятых)
    IFS=',' read -r id year age name movie <<< "$values"
    
    # Удаляем окружающие одинарные кавычки и заменяем '' на '
    name=$(echo "$name" | sed -e "s/^'//" -e "s/'$//" -e "s/''/'/g")
    movie=$(echo "$movie" | sed -e "s/^'//" -e "s/'$//" -e "s/''/'/g")
    
    # Формируем JSON-объект
    printf '{"id":%s, "year":%s, "age":%s, "name":"%s", "movie":"%s"}\n' "$id" "$year" "$age" "$name" "$movie"
done | sed '1s/^/[/;$!s/$/,/;$s/$/]/'
