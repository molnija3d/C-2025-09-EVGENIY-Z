#include "storage.h"

/** 
 * Вспомогательная функция для рекурсивного создания директорий
 * 
 * @param  *path -полный путь до файла (без имени файла)
 * @param  return успех/ошибка
 */
static int mkdir_p(const char *path) {
    char tmp[PATH_LEN];
    char *p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/**
 * Создает и инициализирует объект хранилища 
 * 
 * @param *cfg - указатель на конфигурацию
 * @param *tor - указатель на структуру, описывающую торрент
 * @return storage_t* - возвращает указатель на инициализированный объект хранилища
 */
storage_t *storage_open(const config_t *cfg, const torrent_t *tor) {
    if (cfg->output_file && tor->file_count > 1) {
        LOG_ERROR("Multi-file torrent cannot be saved to a single file. Use -O <directory>");
        return NULL;
    }
    storage_t *st = xcalloc(1, sizeof(storage_t));
    st->file_count = tor->file_count;
    st->files = xcalloc(st->file_count, sizeof(file_info_t));
    st->total_length = tor->total_length;
    st->piece_length = tor->piece_length;
    st->extract_dir = cfg->extract_dir; // может быть NULL

    uint64_t current_offset = 0;
    for (size_t i = 0; i < tor->file_count; i++) {
        file_info_t *fi = &st->files[i];
        const file_t *tf = &tor->files[i];
         // offset - конец предыдущего файла
        fi->offset = current_offset;
        fi->length = tf->length;

        // Построение полного пути
        // Используем динамический буфер
        char full_path[PATH_LEN] = {0};
        if (st->extract_dir) {
            strncpy(full_path, st->extract_dir, sizeof(full_path) - 1);
            strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
        }
        // path_len - количество компонентов пути (directory, file name и т.д.)
        for (size_t j = 0; j < tf->path_len; j++) {
            strncat(full_path, tf->path[j], sizeof(full_path) - strlen(full_path) - 1);
            if (j < tf->path_len - 1) {
                strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
            }
        }

        fi->full_path = xmalloc(strlen(full_path) + 1);
        strcpy(fi->full_path, full_path);

        // Создаём директории для этого файла (если нужно)
        char *last_slash = strrchr(fi->full_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (mkdir_p(fi->full_path) != 0) {
                LOG_ERROR("Failed to create directory %s", fi->full_path);
                // Восстанавливаем slash для дальнейшего использования
                *last_slash = '/';
                storage_close(st);
                return NULL;
            }
            *last_slash = '/';
        }

        // Открываем файл на запись (создаём или перезаписываем)
        fi->fp = fopen(fi->full_path, "wb");
        if (!fi->fp) {
            LOG_ERROR("Failed to open file %s: %s", fi->full_path, strerror(errno));
            storage_close(st);
            return NULL;
        }

        current_offset += fi->length;
    }
    return st;
}

/**
 * Записывает в файл полученные данные(или их часть) начиная с нужной позиции
 *
 * @param  *st - указатель на структуру хранилища, вней хронится список файлов и их параметры (дескриптор, путь, размер, имя и т.д.)
 * @param  piece_index - номер части данных во входящем потоке
 * @param  *data - данные
 * @param  len - длина данных
 */
void storage_write(storage_t *st, uint32_t piece_index, const uint8_t *data, uint32_t len) {
    uint64_t piece_start = (uint64_t)piece_index * st->piece_length;
    uint64_t piece_end = piece_start + len;
    // проходимся по всему массиву файлов
    for (size_t i = 0; i < st->file_count; i++) {
        file_info_t *fi = &st->files[i];
        uint64_t file_start = fi->offset;
        uint64_t file_end = fi->offset + fi->length;

        // Вычисляем пересечение куска с файлом
        uint64_t overlap_start = piece_start > file_start ? piece_start : file_start;
        uint64_t overlap_end = piece_end < file_end ? piece_end : file_end;

        if (overlap_start < overlap_end) {
            // Есть перекрытие
            uint64_t offset_in_file = overlap_start - file_start;
            uint64_t offset_in_piece = overlap_start - piece_start;
            size_t bytes_to_write = overlap_end - overlap_start;

            // Позиционируемся в файле и пишем
            if (fseek(fi->fp, offset_in_file, SEEK_SET) != 0) {
                LOG_ERROR("fseek error in file %s", fi->full_path);
                continue;
            }
            size_t written = fwrite(data + offset_in_piece, 1, bytes_to_write, fi->fp);
            if (written != bytes_to_write) {
                LOG_ERROR("Short write in file %s", fi->full_path);
            }
        }
    }
}

/**
 * Совобождение памяти объекта хранилища
 *
 * @param *st -указатель на объект хранилища
 */
void storage_close(storage_t *st) {
    if (!st) return;
    for (size_t i = 0; i < st->file_count; i++) {
        if (st->files[i].fp) fclose(st->files[i].fp);
        xfree(st->files[i].full_path);
    }
    xfree(st->files);
    xfree(st);
}
