#include "logger.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>


#define BACKTRACE_SIZE 128

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; 
static FILE* log_file = NULL;
static LogLevel current_level = LOG_INFO;
static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR"
};

static const char* level_colors[] = {
    "\033[36m",  // CYAN for DEBUG
    "\033[32m",  // GREEN for INFO
    "\033[33m",  // YELLOW for WARNING
    "\033[31m"   // RED for ERROR
};

void log_init(const char* filename) {
    if (log_file) {
        fclose(log_file);
    }
    /*
     * if cant open a file use stderr for logging
     */
    
    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            fprintf(stderr, "Cannot open log file: %s\n", filename);
            log_file = stderr;
        }
    } else {
        log_file = stderr;
    }
    
    setvbuf(log_file, NULL, _IOLBF, 0);
}

void log_close() {
    if (log_file && log_file != stderr && log_file != stdout) {
        fclose(log_file);
    }
    log_file = NULL;
}

void log_set_level(LogLevel level) {
    current_level = level;
}

static void print_backtrace() {
    void* buffer[BACKTRACE_SIZE];
    int size = backtrace(buffer, BACKTRACE_SIZE);
    char** symbols = backtrace_symbols(buffer, size);
    
    if (symbols) {
        fprintf(log_file, "Backtrace:\n");
        /*
         * i = 1 to skip calling backtrace
         */
        for (int i = 1; i < size; i++) {   
            fprintf(log_file, "  %2d: %s\n", i, symbols[i]);
        }
        free(symbols);
    }
}

void log_message(LogLevel level, const char* file, int line, const char* func, const char* fmt, ...) {
        /*
         * Using mutex for multi thread safety
         */
    pthread_mutex_lock(&log_mutex);
    if (level < current_level) {
        return;
    }
    
    if (!log_file) {
        log_file = stderr;
    }
    
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    const char* filename = strrchr(file, '/');
    if (!filename) {
        filename = strrchr(file, '\\');
    }
    if (filename) {
        filename++;
    } else {
        filename = file;
    }
    
    /*
     * Colored output
     */
    int is_tty = isatty(fileno(log_file));
    if (is_tty && level >= LOG_DEBUG && level <= LOG_ERROR) {
        fprintf(log_file, "%s", level_colors[level]);
    }
    
    fprintf(log_file, "[%s] [%s] [%s:%d:%s] ", 
            timestamp, level_strings[level], filename, line, func);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    
    /*
     * Reset color for terminal
     */
    if (is_tty && level >= LOG_DEBUG && level <= LOG_ERROR) {
        fprintf(log_file, RESET_TERMINAL);
    }
    
    if (level == LOG_ERROR) {
        print_backtrace();
    }
    
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}
