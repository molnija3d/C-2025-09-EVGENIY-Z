#ifndef UTILS_H
#define UTILS_H

#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdarg.h>
#include <bits/getopt_core.h>

#define LOG_ERROR(...)   log_error(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)    log_warn(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)    log_info(__VA_ARGS__)
#ifdef DEBUG
#define LOG_DEBUG(...)   log_debug(__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)   ((void)0)
#endif

#define TORRENT_BUFFER_CAPACITY 4096

// макросы для работы с битовыми полями (обмен данными с торрент-трекером)
#define IS_DONE(pieces, idx) ((pieces)[(idx)/8] & (1 << (7 - ((idx)%8))))
#define MARK_DONE(pieces, idx) ((pieces)[(idx)/8]  |= (1<< (7 - ((idx)%8))))


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

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *data, size_t size);
void xfree(void *ptr);

// Сигналы
extern volatile int running;
void setup_signals(void);

// Чтение всего файла в память
size_t read_file(const char *path, void **data);
// Чтение из конвеера
size_t read_stdin(uint8_t **out);

/*
 * Логирование в stderr
 * */
static inline void log_error(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "[ERROR] %s:%d ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static inline void log_warn(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "[WARNING] %s:%d ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static inline void log_info(const char *fmt, ...) {
    fprintf(stderr, "[INFO] ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static inline void log_debug(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "[DEBUG] %s:%d ", file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void parse_args(int argc, char **argv, config_t *cfg);
void free_config(config_t *cfg);
#endif
