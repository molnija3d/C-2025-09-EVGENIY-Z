#ifndef CONFIG_H
#define CONFIG_H

#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <bits/getopt_core.h>
#include "utils.h"

#define CONNECTIOIN_TIMEOUT 10000
#define UNCHOKE_TIMEOUT 30000
#define RECEIVE_TIMEOUT 30000

#define DEBUG //расширенный вывод логов stderr

typedef struct {
    char *input_file;      // путь к .torrent файлу
    char *watch_dir;       // директория для отслеживания
    char *output_file;     // выходной файл (tar)
    char *extract_dir;     // директория для извлечения
    void *out_ctx;         // указатель на контекст вывода
    int use_stdin;         // читать из stdin
    int use_stdout;        // писать в stdout
    int use_tar;           // Использовать tar - 1, не использовать - 0
} config_t;

void parse_args(int argc, char **argv, config_t *cfg);
void free_config(config_t *cfg);

#endif
