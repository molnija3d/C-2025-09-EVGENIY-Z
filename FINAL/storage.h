#ifndef STORAGE_H
#define STORAGE_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "config.h"
#include "torrent.h"

typedef struct {
    char *path;      // полный путь к файлу (для создания)
    uint64_t offset; // смещение в общем потоке данных (начало файла)
    uint64_t length;
    FILE *fp;        // открытый файл
} file_info_t;

typedef struct storage {
    file_info_t *files;
    size_t file_count;
    uint64_t total_length;
    uint32_t piece_length;
    // для вывода в stdout или один файл (режим tar) - позже
} storage_t;

storage_t *storage_open(const config_t *cfg, const torrent_t *tor);
void storage_write(storage_t *st, uint32_t piece_index, const uint8_t *data, uint32_t len);
void storage_close(storage_t *st);

#endif
