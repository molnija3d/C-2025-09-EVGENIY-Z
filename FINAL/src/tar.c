#include "tar.h"

#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define TAR_BLOCK_SIZE 512

/** 
 * Запись значения в восьмеричную строку с завершающим нулём и пробелом (utar)
 *
 * @val - числовое значение
 * @*buf - указатель на буфер для записи
 * @size - размер буфера
 * */
static void oct_to_str(uint64_t val, char *buf, int size) {
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%0%do ", size - 2); // например, "%011o " для size=12
    snprintf(buf, size, fmt, (unsigned int)val);
}

/** 
 * Вычисление контрольной суммы заголовка 
 *
 * @*hdr - указатель на заголовок tar_header_t
 * @return - контрольная сумма
 */
static unsigned int calculate_checksum(const tar_header_t *hdr) {
    const unsigned char *p = (const unsigned char *)hdr;
    unsigned int sum = 0;
    for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
        sum += p[i];
    }
    return sum;
}

/** 
 * Запись заголовка для файла с именем path и размером size 
 * 
 * @*tw - указатель на объект tar_writer_t
 * @*path - полный путь
 * @size - размер
 * */
static void write_header(tar_writer_t *tw, const char *path, uint64_t size) {
    tar_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));

    size_t len = strlen(path);
    if (len < sizeof(hdr.name)) {
        strcpy(hdr.name, path);
    } else {
        memcpy(hdr.name, path, sizeof(hdr.name) - 1);
    }

    oct_to_str(0644, hdr.mode, sizeof(hdr.mode));
    oct_to_str(0, hdr.uid, sizeof(hdr.uid));
    oct_to_str(0, hdr.gid, sizeof(hdr.gid));
    oct_to_str(size, hdr.size, sizeof(hdr.size));
    oct_to_str(time(NULL), hdr.mtime, sizeof(hdr.mtime));
    hdr.typeflag = '0';
    memcpy(hdr.magic, "ustar", 6);
    memcpy(hdr.version, "00", 2);
    memset(hdr.chksum, ' ', sizeof(hdr.chksum));
    unsigned int chksum = calculate_checksum(&hdr);
    oct_to_str(chksum, hdr.chksum, sizeof(hdr.chksum));

    fwrite(&hdr, TAR_BLOCK_SIZE, 1, tw->out);
    tw->total_written += TAR_BLOCK_SIZE;
}

/**
 * Добавляет паддинг до следующей границы 512 байт, если необходимо
 *
 * @*tw -указатель на объект tar_writer_t
 */
static void write_padding(tar_writer_t *tw) {
    uint64_t mod = tw->total_written % TAR_BLOCK_SIZE;
    if (mod != 0) {
        size_t pad = TAR_BLOCK_SIZE - mod;
        uint8_t zeros[512] = {0};
        fwrite(zeros, 1, pad, tw->out);
        tw->total_written += pad;
    }
}

/**
 * Создает и заполняет структуру для формирования отслеживания файлов tar- архива
 *
 * @*out - укзатель на поток вывода
 * @*tor - указатель на структуру описывающую torrent
 * @return tar_writer_t * - возвращает указатель на структуру для работы с tar-архивом
 */
tar_writer_t *tar_writer_open(FILE *out, const torrent_t *tor) {
    tar_writer_t *tw = xmalloc(sizeof(tar_writer_t));
    memset(tw, 0, sizeof(*tw));
    tw->out = out;
    tw->tor = tor;
    tw->current_file_index = -1;
    return tw;
}

/**
 * Записывает блок данных в поток
 *
 * @tw - указатель на структуру tar_writer (отслеживает сколько осталось записать, интекс текущего файла и тд.)
 * @piece_index - номер блока
 * @*data - указатель на данные блока
 * @len - длина блока
 */
void tar_writer_write(tar_writer_t *tw, uint32_t piece_index, const uint8_t *data, uint32_t len) {
    uint64_t piece_start = (uint64_t)piece_index * tw->tor->piece_length;
    uint64_t piece_end = piece_start + len;

    uint64_t file_global_offset = 0;
    //Проходим по всему спику файлов
    for (size_t i = 0; i < tw->tor->file_count; i++) {
        //сохраняем указатель на текущее описание файла (путь, размер и т.д.)
        const file_t *f = &tw->tor->files[i];
        uint64_t file_start = file_global_offset;
        uint64_t file_end = file_start + f->length;

        uint64_t overlap_start = piece_start > file_start ? piece_start : file_start;
        uint64_t overlap_end = piece_end < file_end ? piece_end : file_end;

        if (overlap_start < overlap_end) {
            uint64_t offset_in_file = overlap_start - file_start;
            uint64_t offset_in_piece = overlap_start - piece_start;
            size_t bytes = overlap_end - overlap_start;

            // Если перешли к новому файлу
            if ((int)i != tw->current_file_index) {
                // Записываем заголовок нового файла
                char full_path[4096] = {0};
                for (size_t k = 0; k < f->path_len; k++) {
                    strcat(full_path, f->path[k]);
                    if (k < f->path_len - 1) strcat(full_path, "/");
                }
                write_header(tw, full_path, f->length);
                tw->current_file_index = i;
                tw->file_bytes_written = 0;
            }

            // Пишем данные (без выравнивания)
            fwrite(data + offset_in_piece, 1, bytes, tw->out);
            tw->total_written += bytes;
            tw->file_bytes_written += bytes;

            // Если это последний фрагмент текущего файла (достигнут конец файла)
            if (offset_in_file + bytes == f->length) {
                write_padding(tw); // выравниваем до 512 после файла
                // Файл завершён, но current_file_index остаётся прежним до следующего переключения
            }
        }

        file_global_offset += f->length;
    }
}

/**
 * Освобождение памяти под объект tar_writer_t
 *
 * @*tw -указатель на объект
 */
void tar_writer_close(tar_writer_t *tw) {
    if (!tw) return;

    // Если последний файл не был завершён 
    if (tw->current_file_index >= 0) {
        // Проверяем, нужно ли добавить паддинг для последнего файла
        // (если он не был добавлен ранее)
        const file_t *f = &tw->tor->files[tw->current_file_index];
        if (tw->file_bytes_written < f->length) {
            // Файл не завершён, добавим паддинг от текущей позиции
            write_padding(tw);
        }
    }

    // Два нулевых блока в конце архива
    uint8_t zeros[1024] = {0};
    fwrite(zeros, 1, 1024, tw->out);
    fflush(tw->out);
    free(tw);
}
