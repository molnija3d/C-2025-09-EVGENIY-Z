#include "config.h"
/**
 * Обработка аргументов командной строки с помощью getopt, заполняет стрктуру cfg
 *
 * @param argc - количество параметров
 * @param **argv - указатель на список аргументов
 * @param *cfg - структрура для заполнения параметров конфигурации
 */
void parse_args(int argc, char **argv, config_t *cfg) {
    memset(cfg, 0, sizeof(config_t));
    cfg->use_stdin = 1;
    cfg->use_stdout = 1;
    int opt;
    while ((opt = getopt(argc, argv, "f:d:o:O:")) != -1) {
        switch (opt) {
        case 'f':
            cfg->input_file = strdup(optarg);
            cfg->use_stdin = 0;
            break;
        case 'd':
            cfg->watch_dir = strdup(optarg);
            cfg->use_stdin = 0;
            break;
        case 'o':
            cfg->output_file = strdup(optarg);
            cfg->use_stdout = 0;
            break;
        case 'O':
            cfg->extract_dir = strdup(optarg);
            cfg->use_stdout = 0;
            break;
        default:
            LOG_ERROR("Usage: %s [-f file.torrent | -d dir] [-o file | -O dir]\n", argv[0]);
            exit(1);
        }
    }
}

/**
 * Освобождение памяти под внутреннее содержимое структуры конфигурации
 *
 * @param *cfg - указатель на структуру
 */
void free_config(config_t *cfg) {
    free(cfg->input_file);
    free(cfg->watch_dir);
    free(cfg->output_file);
    free(cfg->extract_dir);
}

