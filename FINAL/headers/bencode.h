#ifndef BENCODE_H
#define BENCODE_H

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>

// Типы объектов ben_obj (строка, число, список, словарь)
typedef enum {
    BEN_STRING,
    BEN_INT,
    BEN_LIST,
    BEN_DICT
} ben_type_t;

// Каркас объекта ben_obj
typedef struct ben_obj {
    ben_type_t type;
    union {
        struct { uint8_t *data; size_t len; } string;
        int64_t integer;
        struct { struct ben_obj *items; size_t count; } list;
        struct { struct ben_pair *pairs; size_t count; } dict;
    } value;
} ben_obj_t;

// Пара ключ:значение
typedef struct ben_pair {
    char *key;          // ключ как null-терминированная строка
    size_t key_len;
    ben_obj_t *value;
} ben_pair_t;

// Динамический буфер
typedef struct {  
    uint8_t *data;  // указатель на данные
    size_t len;    // длина данных
    size_t cap;   // размер динамического буфера
} dynbuf_t;

// Декодирование
ben_obj_t *bencode_decode(const uint8_t *data, size_t size);
void bencode_free(ben_obj_t *obj);
ben_obj_t *bencode_dict_get(const ben_obj_t *dict, const char *key);
const uint8_t *bencode_string_data(const ben_obj_t *obj, size_t *len);
int64_t bencode_int_value(const ben_obj_t *obj);

// Кодирование (возвращает новый буфер)
uint8_t *bencode_encode(const ben_obj_t *obj, size_t *out_len);

#endif
