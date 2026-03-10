#ifndef TAR_H
#define TAR_H

#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include "torrent.h"
#include "utils.h"
#define TAR_BLOCK_SIZE 512

/* Формат заголовка ustar (512 байт) */
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} tar_header_t;

/* Внутренняя структура для отслеживания текущего файла */
typedef struct {
    const torrent_t *tor;          // ссылка на торрент (не владеем)
    FILE *out;                     // выходной поток
    int current_file_index;        // индекс текущего файла в tor->files
    uint64_t file_bytes_written; // сколько байт текущего файла уже записано
    uint64_t total_written;   // общее количество байт, записанных в архив (для расчёта паддинга)
} tar_writer_t;

/**
 * Создаёт контекст для записи tar-архива в указанный поток (обычно stdout).
 * @param out   Файловый поток для вывода (например, stdout)
 * @param tor   Торрент, для которого создаётся архив (нужен для получения списка файлов)
 * @return      Указатель на контекст или NULL при ошибке
 */
tar_writer_t *tar_writer_open(FILE *out, const torrent_t *tor);

/**
 * Записывает очередной кусок данных в tar-архив.
 * Функция автоматически распределяет данные по файлам в соответствии с их смещениями.
 * @param tw    Контекст tar-писателя
 * @param piece_index   Номер куска
 * @param data          Указатель на данные куска
 * @param len           Размер данных куска (обычно равен piece_size)
 */
void tar_writer_write(tar_writer_t *tw, uint32_t piece_index, const uint8_t *data, uint32_t len);

/**
 * Завершает запись tar-архива: дописывает выравнивание для последнего файла
 * и два нулевых блока в конец.
 * @param tw    Контекст
 */
void tar_writer_close(tar_writer_t *tw);
#endif
