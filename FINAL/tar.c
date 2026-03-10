#include "tar.h"

#include "tar.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define TAR_BLOCK_SIZE 512


/* Преобразование числа в восьмеричную строку с завершающим нулём и пробелом */
static void oct_to_str(uint64_t val, char *buf, int size) {
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%0%do ", size - 2); // например, "%011o " для size=12
    snprintf(buf, size, fmt, (unsigned int)val);
}

/* Вычисление контрольной суммы заголовка */
static unsigned int calculate_checksum(const tar_header_t *hdr) {
    const unsigned char *p = (const unsigned char *)hdr;
    unsigned int sum = 0;
    for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
        sum += p[i];
    }
    return sum;
}

/* Запись одного блока (заголовка или данных) с выравниванием до TAR_BLOCK_SIZE */
static void write_block(FILE *out, const void *data, size_t len) {
    fwrite(data, 1, len, out);
    size_t pad = (TAR_BLOCK_SIZE - (len % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
    if (pad) {
        uint8_t zeros[512] = {0};
        fwrite(zeros, 1, pad, out);
    }
}

/* Запись заголовка для файла с именем path и размером size */
static void write_header(FILE *out, const char *path, uint64_t size) {
    tar_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));

    // Имя файла (до 100 символов) - для простоты обрезаем, если длиннее
    size_t len = strlen(path);
    if (len < sizeof(hdr.name)) {
        strcpy(hdr.name, path);
    } else {
        memcpy(hdr.name, path, sizeof(hdr.name) - 1);
        hdr.name[sizeof(hdr.name) - 1] = '\0';
    }

    // Режим доступа (644)
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

    fwrite(&hdr, TAR_BLOCK_SIZE, 1, out);
}

tar_writer_t *tar_writer_open(FILE *out, const torrent_t *tor) {
    tar_writer_t *tw = xmalloc(sizeof(tar_writer_t));
    memset(tw, 0, sizeof(*tw));
    tw->out = out;
    tw->tor = tor;
    tw->current_file_index = -1;
    return tw;
}

void tar_writer_write(tar_writer_t *tw, uint32_t piece_index, const uint8_t *data, uint32_t len) {
    uint64_t piece_start = (uint64_t)piece_index * tw->tor->piece_length;
    uint64_t piece_end = piece_start + len;

    // Вычисляем смещения файлов на лету
    uint64_t file_global_offset = 0;
    for (size_t i = 0; i < tw->tor->file_count; i++) {
        const file_t *f = &tw->tor->files[i];
        uint64_t file_start = file_global_offset;
        uint64_t file_end = file_start + f->length;

        uint64_t overlap_start = piece_start > file_start ? piece_start : file_start;
        uint64_t overlap_end = piece_end < file_end ? piece_end : file_end;

        if (overlap_start < overlap_end) {
            // Есть пересечение
            uint64_t offset_in_file = overlap_start - file_start;
            uint64_t offset_in_piece = overlap_start - piece_start;
            size_t bytes_to_write = overlap_end - overlap_start;

            // Если перешли к новому файлу (и ещё не записывали его заголовок)
            if ((int)i != tw->current_file_index) {
                char full_path[4096] = {0};
                for (size_t k = 0; k < f->path_len; k++) {
                    strcat(full_path, f->path[k]);
                    if (k < f->path_len - 1) strcat(full_path, "/");
                }
                write_header(tw->out, full_path, f->length);
                tw->current_file_index = i;
                tw->file_bytes_written = 0;
            }

            // Записываем фрагмент данных (write_block автоматически выравнивает до 512)
            write_block(tw->out, data + offset_in_piece, bytes_to_write);
            tw->file_bytes_written += bytes_to_write;
        }

        file_global_offset += f->length; // переходим к следующему файлу
    }
}

void tar_writer_close(tar_writer_t *tw) {
    if (!tw) return;
    // Два нулевых блока в конце архива
    uint8_t zeros[1024] = {0};
    fwrite(zeros, 1, 1024, tw->out);

    fflush(tw->out);
    free(tw);
}
