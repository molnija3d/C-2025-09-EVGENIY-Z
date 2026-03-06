#ifndef TORRENT_H
#define TORRENT_H

#define _POSIX_C_SOURCE 200809L
#include "bencode.h"
#include "utils.h"
#include <string.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

// Структура для одного файла внутри торрента
typedef struct {
    char **path;         // список компонентов пути (например, {"dir", "subdir", "file.txt", NULL})
    size_t path_len;     // количество компонентов
    uint64_t length;     // размер файла в байтах
} file_t;

// Основная структура торрента
typedef struct {
    // Основные поля
    char *announce;               // announce URL
    char *comment;                // комментарий (может быть NULL)
    int64_t creation_date;        // дата создания (может быть 0)
    char *created_by;             // создатель (может быть NULL)

    // Информация о раздаче (info dict)
    uint8_t info_hash[20];        // SHA1 от закодированного info-словаря
    char *name;                   // имя торрента (для single-file это имя файла, для multi-file — имя корневой директории)
    uint32_t piece_length;        // размер куска в байтах
    uint32_t num_pieces;          // количество кусков
    uint8_t *pieces;              // массив SHA1-хешей кусков (20 * num_pieces байт)

    // Файлы
    file_t *files;                // массив файлов
    size_t file_count;            // количество файлов

    // Общий размер всех файлов (сумма length)
    uint64_t total_length;
} torrent_t;

// Загрузка торрента из файла
int torrent_load(const char *filename, torrent_t *tor);

// Загрузка торрента из буфера в памяти (данные должны оставаться валидными до вызова torrent_free,
// так как структура может ссылаться на них? Нет, мы копируем все нужные данные)
int torrent_load_from_memory(const uint8_t *data, size_t size, torrent_t *tor);

// Освобождение ресурсов, занятых торрентом
void torrent_free(torrent_t *tor);

// Получить размер i-го куска в байтах (последний кусок может быть меньше)
uint32_t piece_size(const torrent_t *tor, uint32_t index);

// Проверить, совпадает ли хеш куска с ожидаемым
int verify_piece(const torrent_t *tor, uint32_t index, const uint8_t *data);

#endif
