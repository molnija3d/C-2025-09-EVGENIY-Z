#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdio.h>

#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void xfree(void *ptr);

// Сигналы
extern volatile int running;
void setup_signals(void);

// Чтение всего файла в память
size_t read_file(const char *path, void **data);
#endif
