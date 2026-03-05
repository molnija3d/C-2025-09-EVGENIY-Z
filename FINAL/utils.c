#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

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

void xfree(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

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
