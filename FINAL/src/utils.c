#include "utils.h"

volatile int running = 1;

/**
 * Обработчик сигналов
 * 
 * @param sig - номер сигнала
 */
static void signal_handler(int sig) {
    (void)sig;
    LOG_ERROR("Received SIGNAL %d. Exiting...", sig);
    running = 0;
}

/**
 * Настраивает обработку сигналов
 *
 */
void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
}

/**
 * Набор функций для работы с памятью (xmalloc, xcalloc, xrealloc, xfree)
 * В отличии от библиотечных, в этих функциях проверяется выделилась памят или нет. 
 * Выводится ошибка в stderr и завершается работа программы
 *
 */
void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror("malloc");
        exit(1);
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        perror("calloc");
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *data, size_t size) {
    void *ptr = realloc(data, size);
    if (!ptr) {
        perror("realloc");
        exit(1);
    }
    return ptr;
}

void xfree(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}


/**
 * Читает данные из файла в буфер в памяти
 *
 * @param *path - указатель на путь
 * @param **data - указатель на указатель на буфер в памяти
 * @return - размер данных
 */
size_t read_file(const char *path, void **data) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    *data = xmalloc(size);
    fread(*data, 1, size, f);
    fclose(f);
    return size;
}

/**
 * Читает данные из входящего потока и записывает в буфер
 * 
 * @param **out - указатель на указатель на буфер в памяти
 * @return - размер данных
 */
size_t read_stdin(uint8_t **out) {
    size_t capacity = TORRENT_BUFFER_CAPACITY;
    size_t size = 0;
    uint8_t *buf = xmalloc(capacity);
    int n;
    while ((n = fread(buf + size, 1, capacity - size, stdin)) > 0) {
        size += n;
        if (size == capacity) {
            capacity *= 2;
            buf = realloc(buf, capacity);
        }
    }
    if (ferror(stdin)) {
        perror("fread");
        free(buf);
        return 0;
    }
    *out = buf;
    return size;
}
