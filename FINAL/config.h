#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char *input_file;      // путь к .torrent файлу
    char *watch_dir;       // директория для отслеживания
    char *output_file;     // выходной файл (tar)
    char *extract_dir;     // директория для извлечения
    int use_stdin;         // читать из stdin
    int use_stdout;        // писать в stdout
} config_t;

void parse_args(int argc, char **argv, config_t *cfg);
void free_config(config_t *cfg);

#endif
