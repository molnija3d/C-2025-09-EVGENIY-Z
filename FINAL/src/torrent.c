#define _POSIX_C_SOURCE 200809L
#include "torrent.h"
#include "bencode.h"
#include "utils.h"
#include <string.h>
#include <openssl/sha.h>
#include <stdlib.h>

/**
 * Вспомогательная функция для освобождения массива строк
 *
 * @param **arr указатель на масссив строк
 * @param len длина
 */
static void free_str_array(char **arr, size_t len) {
    if (!arr) return;
    for (size_t i = 0; i < len; i++) free(arr[i]);
    free(arr);
}

/**
 * Освобождение одного file_t
 *
 * @param указатель на структуру, описывающую файл
 */
static void free_file(file_t *f) {
    if (!f) return;
    free_str_array(f->path, f->path_len);
}

/**
 * Освобождение всех файлов
 *
 * @param указатель на массив файлов
 * @param число файлов (элементов массива)
 */
static void free_files(file_t *files, size_t count) {
    if (!files) return;
    for (size_t i = 0; i < count; i++) free_file(&files[i]);
    free(files);
}

/**
* Рекурсивное копирование строки из bencode-строки (с добавлением нуля)
*
* @param указатель на объект bencode
* @return возвращает строку из объекта bencode
*/

static char *str_from_bencode(const ben_obj_t *obj) {
    if (!obj || obj->type != BEN_STRING) return NULL;
    size_t len;
    const uint8_t *data = bencode_string_data(obj, &len);
    char *s = xmalloc(len + 1);
    memcpy(s, data, len);
    s[len] = '\0';
    return s;
}

/**
 * Парсинг списка path из bencode-листа (каждый элемент — строка)
 * @param *list - указатель на список в формате ben_obj_t
 * @param *out_len указатель на длину пути
 * @return указатель на путь
 */
static char **parse_path_list(const ben_obj_t *list, size_t *out_len) {
    if (!list || list->type != BEN_LIST) return NULL;
    size_t count = list->value.list.count;
    char **path = xcalloc(count + 1, sizeof(char*)); // +1 для NULL (опционально)
    for (size_t i = 0; i < count; i++) {
        ben_obj_t *item = &list->value.list.items[i];
        if (item->type != BEN_STRING) {
            // Если элемент не строка — ошибочный торрент, освобождаем уже выделенное
            for (size_t j = 0; j < i; j++) free(path[j]);
            free(path);
            return NULL;
        }
        path[i] = str_from_bencode(item);
    }
    *out_len = count;
    return path;
}
/**
 * Основная функция загрузки. Заполняет структуру torrent_t *tor данными из torrent-файла
 *
 * @param *data - указатель на данные
 * @param size - размер данных
 * @param *tor - указатель на структуру для заполнения
 * @return успех/ошибка (0/-1)
 */

int torrent_load_from_memory(const uint8_t *data, size_t size, torrent_t *tor) {
    memset(tor, 0, sizeof(torrent_t));

    // Декодируем весь торрент
    ben_obj_t *root = bencode_decode(data, size);
    if (!root || root->type != BEN_DICT) {
        goto load_failure_root;
    }

    // Извлекаем announce
    ben_obj_t *ann = bencode_dict_get(root, "announce");
    if (ann) tor->announce = str_from_bencode(ann);

    // Извлекаем comment (опционально)
    ben_obj_t *com = bencode_dict_get(root, "comment");
    if (com) tor->comment = str_from_bencode(com);

    // Извлекаем creation date
    ben_obj_t *cd = bencode_dict_get(root, "creation date");
    if (cd && cd->type == BEN_INT) tor->creation_date = bencode_int_value(cd);

    // Извлекаем created by
    ben_obj_t *cb = bencode_dict_get(root, "created by");
    if (cb) tor->created_by = str_from_bencode(cb);

    // Извлекаем info-словарь
    ben_obj_t *info = bencode_dict_get(root, "info");
    if (!info || info->type != BEN_DICT) {
        goto load_failure_tor;
    }

    // Вычисляем info_hash: кодируем info-словарь в bencode и берём SHA1
    size_t info_enc_len;
    uint8_t *info_enc = bencode_encode(info, &info_enc_len);
    if (!info_enc) {
        goto load_failure_tor;
    }
    SHA1(info_enc, info_enc_len, tor->info_hash);
    free(info_enc); // нам больше не нужен

    // Извлекаем name
    ben_obj_t *name = bencode_dict_get(info, "name");
    if (name) tor->name = str_from_bencode(name);

    // Извлекаем piece length
    ben_obj_t *pl = bencode_dict_get(info, "piece length");
    if (pl && pl->type == BEN_INT) tor->piece_length = (uint32_t)bencode_int_value(pl);

    // Извлекаем pieces (строка с хешами)
    ben_obj_t *pcs = bencode_dict_get(info, "pieces");
    if (pcs && pcs->type == BEN_STRING) {
        size_t pcs_len;
        const uint8_t *pcs_data = bencode_string_data(pcs, &pcs_len);
        if (pcs_len % 20 != 0) {
            // Неправильная длина
            goto load_failure_tor;
        }
        tor->num_pieces = pcs_len / 20;
        tor->pieces = xmalloc(pcs_len);
        memcpy(tor->pieces, pcs_data, pcs_len);
    }

    // разбираем файлы: смотрим, есть ли ключ "files" (multi-file) или "length" (single-file)
    ben_obj_t *files_list = bencode_dict_get(info, "files");
    if (files_list && files_list->type == BEN_LIST) {
        // Multi-file режим
        size_t file_count = files_list->value.list.count;
        tor->file_count = file_count;
        tor->files = xcalloc(file_count, sizeof(file_t));
        tor->total_length = 0;

        for (size_t i = 0; i < file_count; i++) {
            ben_obj_t *file_dict = &files_list->value.list.items[i];
            if (file_dict->type != BEN_DICT) {
                // Ошибка: элемент списка не словарь
                goto load_failure_tor;
            }

            // Извлекаем length
            ben_obj_t *len_obj = bencode_dict_get(file_dict, "length");
            if (!len_obj || len_obj->type != BEN_INT) {
                goto load_failure_tor;
            }
            uint64_t file_len = bencode_int_value(len_obj);
            tor->files[i].length = file_len;
            tor->total_length += file_len;

            // Извлекаем path (список строк)
            ben_obj_t *path_obj = bencode_dict_get(file_dict, "path");
            if (!path_obj || path_obj->type != BEN_LIST) {
                goto load_failure_tor;
            }
            size_t path_len;
            char **path = parse_path_list(path_obj, &path_len);
            if (!path) {
                goto load_failure_tor;
            }
            tor->files[i].path = path;
            tor->files[i].path_len = path_len;
        }
    } else {
        // Single-file режим: используем ключи "length" и "name" (name уже есть)
        ben_obj_t *len_obj = bencode_dict_get(info, "length");
        if (!len_obj || len_obj->type != BEN_INT) {
            // Нет ни files, ни length — ошибочный торрент
            goto load_failure_tor;
        }
        tor->file_count = 1;
        tor->files = xcalloc(1, sizeof(file_t));
        tor->total_length = bencode_int_value(len_obj);
        tor->files[0].length = tor->total_length;
        // Имя файла — это name торрента
        // Создаём путь как одноэлементный список
        tor->files[0].path_len = 1;
        tor->files[0].path = xmalloc(sizeof(char*) * 2);
        tor->files[0].path[0] = tor->name ? strdup(tor->name) : strdup("unknown");
        tor->files[0].path[1] = NULL;
    }

    bencode_free(root);
    return 0;

load_failure_tor: 
    torrent_free(tor); // очистка, при ошибках в данных
load_failure_root:
    bencode_free(root); // ошибка выделения памяти под корень
    return -1;

}

/**
 * Загрузка торрента из файла
 *
 * @param  *filename - указатель на путь к файлу
 * @param  *tor - указатель на структуру, которую надо заполнимить
 * @return - успех/ошибка
 */
int torrent_load(const char *filename, torrent_t *tor) {
    void *data;
    size_t size = read_file(filename, &data);
    if (!size) return -1;
    int ret = torrent_load_from_memory(data, size, tor);
    free(data);
    return ret;
}

/**
 * Усвобождает структуру с описание торрента
 *
 * @param *tor указатель на структуру torrent_t
 */
void torrent_free(torrent_t *tor) {
    if (tor->announce) free(tor->announce);
    if (tor->comment) free(tor->comment);
    if (tor->created_by) free(tor->created_by);
    if (tor->name) free(tor->name);
    if (tor->pieces) free(tor->pieces);
    if (tor->files) free_files(tor->files, tor->file_count);
    memset(tor, 0, sizeof(torrent_t));
}

/**
 * Отправляет размер куска для скачивания по номеру индекса. Эти данные уже загружены в структуру описывающую торрент ранее.
 *
 * @param *tor указатель на структуру torrent_t
 * @param index номер индекса
 * @return размер куска данных 
 */
uint32_t piece_size(const torrent_t *tor, uint32_t index) {
    if (index < tor->num_pieces - 1) {
        return tor->piece_length;
    } else if (index == tor->num_pieces - 1) {
        uint32_t last = tor->total_length % tor->piece_length;
        return last ? last : tor->piece_length;
    }
    return 0;
}

/**
 * Проверка куска данных на целостность с помощью SHA1
 * 
 * @param *tor указатель на torrent_t с описанием торрента
 * @param index номер куска данных
 * @param *data указатель на данные для сранвения
 * @return успех/ошибка (1/0)
 */
int verify_piece(const torrent_t *tor, uint32_t index, const uint8_t *data) {
    if (index >= tor->num_pieces) return 0;
    uint8_t hash[20];
    SHA1(data, piece_size(tor, index), hash);
    return memcmp(hash, tor->pieces + index * 20, 20) == 0;
}
