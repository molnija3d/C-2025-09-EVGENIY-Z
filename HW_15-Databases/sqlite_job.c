#define _GNU_SOURCE   // для math.h с INFINITY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <sqlite3.h>

/**
 * Программа для вычисления статистических параметров по колонке таблицы SQLite.
 * Использование: ./program <db_file> <table_name> <column_name>
 * Вычисляет: количество, сумму, минимум, максимум, среднее, дисперсию (смещённую).
 * При обнаружении нечислового значения (NULL или текст, не являющийся числом)
 * программа завершается с сообщением об ошибке.
 */

/**
 * Проверяет, является ли строка числом 
 * Возвращает 1, если строка это число, иначе 0.
 */
static int is_numeric(const char *str, double *out_val) {
    if (str == NULL || *str == '\0')
        return 0;                       // пустая строка или NULL

    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);
    if (errno == ERANGE)
        return 0;                       // переполнение (очень большое/малое)

    while (isspace((unsigned char)*endptr))
        endptr++;

    // Если найдено число и достигнут конец строки
    if (endptr != str && *endptr == '\0') {
        *out_val = val;
        return 1;
    }
    return 0;                            // остались символы, не являющиеся частью числа
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Использование: %s <файл_БД> <таблица> <колонка>\n", argv[0]);
        return 1;
    }

    const char *db_file = argv[1];
    const char *table_name = argv[2];
    const char *column_name = argv[3];

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *query = NULL;
    int rc;

    // Открытие базы данных
    rc = sqlite3_open(db_file, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Не удалось открыть базу данных '%s': %s\n",
                db_file, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return 1;
    }

    // Формирование запроса с экранированием имён таблицы и колонки
    query = sqlite3_mprintf("SELECT \"%w\" FROM \"%w\"", column_name, table_name);
    if (query == NULL) {
        fprintf(stderr, "Ошибка выделения памяти под запрос\n");
        sqlite3_close(db);
        return 1;
    }

    // Подготовка запроса
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Ошибка подготовки запроса:\n  SQL: %s\n  Ошибка: %s\n",
                query, sqlite3_errmsg(db));
        sqlite3_free(query);
        sqlite3_close(db);
        return 1;
    }
    sqlite3_free(query);   // больше не нужен

    // Переменные для сбора статистики
    long long count = 0;
    double sum = 0.0;
    double sum_sq = 0.0;
    double min_val = INFINITY;
    double max_val = -INFINITY;

    // Выполнение запроса и обработка строк
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        // Получаем значение колонки как текст
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        double val;

        // Проверяем, является ли значение числом
        if (!is_numeric((const char*)text, &val)) {
            fprintf(stderr, "Ошибка: обнаружено нечисловое значение в колонке '%s'.\n",
                    column_name);
            if (text == NULL)
                fprintf(stderr, "  (NULL)\n");
            else
                fprintf(stderr, "  \"%s\"\n", text);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return 1;
        }

        // Обновляем статистику
        if (count == 0) {
            min_val = max_val = val;
        } else {
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        sum += val;
        sum_sq += val * val;
        count++;
    }

    // Проверка на ошибку выполнения шага
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Ошибка при выполнении запроса: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_finalize(stmt);

    // Если не найдено ни одной числовой строки
    if (count == 0) {
        fprintf(stderr, "В колонке '%s' нет числовых данных.\n", column_name);
        sqlite3_close(db);
        return 1;
    }

    // Вычисление среднего и дисперсии (смещённая, деление на n)
    double mean = sum / count;
    double variance = (sum_sq / count) - (mean * mean);

    // Вывод результатов
    printf("Количество значений : %lld\n", count);
    printf("Сумма               : %g\n", sum);
    printf("Минимум             : %g\n", min_val);
    printf("Максимум            : %g\n", max_val);
    printf("Среднее             : %g\n", mean);
    printf("Дисперсия           : %g\n", variance);

    sqlite3_close(db);
    return 0;
}
