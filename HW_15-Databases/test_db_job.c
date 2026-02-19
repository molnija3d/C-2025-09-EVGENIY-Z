#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

// SQLite headers
#include <sqlite3.h>

// MongoDB C Driver headers
#include <mongoc/mongoc.h>
#include <bson/bson.h>

/**
 * Проверяет, является ли строка числом 
 */
static int is_numeric(const char *str, double *out_val) {
    if (str == NULL || *str == '\0')
        return 0;

    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);
    if (errno == ERANGE)
        return 0;

    while (isspace((unsigned char)*endptr))
        endptr++;

    if (endptr != str && *endptr == '\0') {
        *out_val = val;
        return 1;
    }
    return 0;
}

/**
 * Структура для хранения статистики
 */
typedef struct {
    long long count;
    double sum;
    double sum_sq;
    double min_val;
    double max_val;
} Statistics;

/**
 * Инициализация статистики
 */
static void stats_init(Statistics *stats) {
    stats->count = 0;
    stats->sum = 0.0;
    stats->sum_sq = 0.0;
    stats->min_val = INFINITY;
    stats->max_val = -INFINITY;
}

/**
 * Обновление статистики очередным значением
 */
static int stats_update(Statistics *stats, double val) {
    if (stats->count == 0) {
        stats->min_val = stats->max_val = val;
    } else {
        if (val < stats->min_val) stats->min_val = val;
        if (val > stats->max_val) stats->max_val = val;
    }
    stats->sum += val;
    stats->sum_sq += val * val;
    stats->count++;
    return 0;
}

/**
 * Вывод статистики
 */
static void stats_print(const Statistics *stats) {
    if (stats->count == 0) {
        printf("Нет числовых данных для обработки\n");
        return;
    }

    double mean = stats->sum / stats->count;
    double variance = (stats->sum_sq / stats->count) - (mean * mean);

    printf("Количество значений : %lld\n", stats->count);
    printf("Сумма               : %g\n", stats->sum);
    printf("Минимум             : %g\n", stats->min_val);
    printf("Максимум            : %g\n", stats->max_val);
    printf("Среднее             : %g\n", mean);
    printf("Дисперсия           : %g\n", variance);
}

/**
 * Обработка SQLite базы данных
 */
static int process_sqlite(const char *db_path, const char *table_name,
                          const char *column_name) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *query = NULL;
    int rc;
    Statistics stats;
    stats_init(&stats);

    // Открытие базы данных
    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Ошибка открытия SQLite БД '%s': %s\n",
                db_path, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return 1;
    }

    // Формирование запроса с экранированием
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
    sqlite3_free(query);

    // Выполнение запроса
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        double val;

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

        stats_update(&stats, val);
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Ошибка при выполнении запроса: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    stats_print(&stats);
    return 0;
}

/**
 * Обработка MongoDB
 * Параметры: строка подключения (например, "mongodb://localhost:27017/"),
 *             имя базы данных, имя коллекции, имя поля
 */
static int process_mongodb(const char *conn_string, const char *db_name,
                           const char *collection_name, const char *field_name) {
    mongoc_client_t *client = NULL;
    mongoc_collection_t *collection = NULL;
    mongoc_cursor_t *cursor = NULL;
    bson_t *query = NULL;
    const bson_t *doc;
    Statistics stats;
    stats_init(&stats);

    // Инициализация MongoDB driver
    mongoc_init();

    // Создание клиента
    client = mongoc_client_new(conn_string);
    if (!client) {
        fprintf(stderr, "Ошибка создания MongoDB клиента для '%s'\n", conn_string);
        mongoc_cleanup();
        return 1;
    }

    // Получение коллекции
    collection = mongoc_client_get_collection(client, db_name, collection_name);
    if (!collection) {
        fprintf(stderr, "Ошибка получения коллекции '%s'\n", collection_name);
        mongoc_client_destroy(client);
        mongoc_cleanup();
        return 1;
    }

    // Создание запроса (получаем все документы)
    query = bson_new();
    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    // Итерация по документам
    while (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;

        // Ищем указанное поле в документе
        if (bson_iter_init_find(&iter, doc, field_name)) {
            double val = 0;
            bool is_number = false;

            // Проверяем тип значения
            if (BSON_ITER_HOLDS_INT32(&iter)) {
                val = bson_iter_int32(&iter);
                is_number = true;
            } else if (BSON_ITER_HOLDS_INT64(&iter)) {
                val = bson_iter_int64(&iter);
                is_number = true;
            } else if (BSON_ITER_HOLDS_DOUBLE(&iter)) {
                val = bson_iter_double(&iter);
                is_number = true;
            } else if (BSON_ITER_HOLDS_UTF8(&iter)) {
                // Попытка преобразовать строку в число
                const char *str_val = bson_iter_utf8(&iter, NULL);
                if (is_numeric(str_val, &val)) {
                    is_number = true;
                } else {
                    fprintf(stderr, "Ошибка: обнаружено нечисловое строковое значение в поле '%s': \"%s\"\n",
                            field_name, str_val);
                    bson_destroy(query);
                    mongoc_cursor_destroy(cursor);
                    mongoc_collection_destroy(collection);
                    mongoc_client_destroy(client);
                    mongoc_cleanup();
                    return 1;
                }
            } else {
                // Другие типы (null, bool, array, etc) считаем нечисловыми
                const char *type_name = "unknown";
                bson_type_t btype = bson_iter_type(&iter);
                switch (btype) {
                case BSON_TYPE_NULL:
                    type_name = "null";
                    break;
                case BSON_TYPE_BOOL:
                    type_name = "bool";
                    break;
                case BSON_TYPE_ARRAY:
                    type_name = "array";
                    break;
                case BSON_TYPE_DOCUMENT:
                    type_name = "document";
                    break;
                default:
                    type_name = "other";
                    break;
                }
                fprintf(stderr, "Ошибка: обнаружено нечисловое значение в поле '%s' (тип: %s)\n",
                        field_name, type_name);
                // ... очистка и выход ...
            }

            if (is_number) {
                stats_update(&stats, val);
            }
        } else {
            fprintf(stderr, "Ошибка: поле '%s' отсутствует в документе\n", field_name);
            bson_destroy(query);
            mongoc_cursor_destroy(cursor);
            mongoc_collection_destroy(collection);
            mongoc_client_destroy(client);
            mongoc_cleanup();
            return 1;
        }
    }

    // Проверка на ошибки курсора
    if (mongoc_cursor_error(cursor, NULL)) {
        fprintf(stderr, "Ошибка при итерации по курсору MongoDB\n");
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_destroy(client);
        mongoc_cleanup();
        return 1;
    }

    // Очистка ресурсов
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();

    stats_print(&stats);
    return 0;
}

/**
 * Вывод справки
 */
static void print_usage(const char *prog_name) {
    fprintf(stderr, "Использование:\n");
    fprintf(stderr, "  SQLite: %s sqlite <файл_БД> <таблица> <колонка>\n", prog_name);
    fprintf(stderr, "  MongoDB: %s mongo <строка_подключения> <база_данных> <коллекция> <поле>\n", prog_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Примеры:\n");
    fprintf(stderr, "  %s sqlite test.db oscar age\n", prog_name);
    fprintf(stderr, "  %s mongo \"mongodb://localhost:27017\" testdb oscar age\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *db_type = argv[1];

    if (strcmp(db_type, "sqlite") == 0) {
        // sqlite <db_file> <table> <column>
        if (argc != 5) {
            print_usage(argv[0]);
            return 1;
        }
        return process_sqlite(argv[2], argv[3], argv[4]);
    }
    else if (strcmp(db_type, "mongo") == 0) {
        // mongo <conn_string> <database> <collection> <field>
        if (argc != 6) {
            print_usage(argv[0]);
            return 1;
        }
        return process_mongodb(argv[2], argv[3], argv[4], argv[5]);
    }
    else {
        fprintf(stderr, "Неизвестный тип базы данных: %s\n", db_type);
        print_usage(argv[0]);
        return 1;
    }
}
