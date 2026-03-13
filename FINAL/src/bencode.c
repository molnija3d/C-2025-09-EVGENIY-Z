#include "bencode.h"

static const uint8_t *parse_int(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj);
static const uint8_t *parse_string(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj);
static const uint8_t *parse_list(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj);
static const uint8_t *parse_dict(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj);


// Вспомогательная функция для динамического расширения буфера
static void dynbuf_init(dynbuf_t *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

/**
 * Добавить в динамический буфер данные
 *
 * *b - указатель на буфер
 * *src - указатель на начало данных
 * n - размер данных
 */
static void dynbuf_append(dynbuf_t *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 128;
        while (new_cap < b->len + n) new_cap *= 2;
        b->data = xrealloc(b->data, new_cap); 
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void dynbuf_append_char(dynbuf_t *b, char c) {
    dynbuf_append(b, &c, 1);
}

static void dynbuf_append_str(dynbuf_t *b, const char *s) {
    dynbuf_append(b, s, strlen(s));
}
/**
 * Заполнение (кодирование) буфера в формате bencode (для расчета info_hash, общения с трекером)
 *
 * @ *b - динамический буфер
 * @ *obj - bencode объект с данными (после парсинга торрент-файла)
 */
static void encode_obj(dynbuf_t *b, const ben_obj_t *obj) {
    char tmp[32];
    switch (obj->type) {
    case BEN_STRING:
        snprintf(tmp, sizeof(tmp), "%zu:", obj->value.string.len);
        dynbuf_append_str(b, tmp);
        dynbuf_append(b, obj->value.string.data, obj->value.string.len);
        break;
    case BEN_INT:
        snprintf(tmp, sizeof(tmp), "i%llde", (long long)obj->value.integer);
        dynbuf_append_str(b, tmp);
        break;
    case BEN_LIST:
        dynbuf_append_char(b, 'l');
        for (size_t i = 0; i < obj->value.list.count; i++) {
            encode_obj(b, &obj->value.list.items[i]);
        }
        dynbuf_append_char(b, 'e');
        break;
    case BEN_DICT:
        dynbuf_append_char(b, 'd');
        for (size_t i = 0; i < obj->value.dict.count; i++) {
            const ben_pair_t *pair = &obj->value.dict.pairs[i];
            // ключ как строка
            snprintf(tmp, sizeof(tmp), "%zu:", pair->key_len);
            dynbuf_append_str(b, tmp);
            dynbuf_append(b, pair->key, pair->key_len);
            // значение
            encode_obj(b, pair->value);
        }
        dynbuf_append_char(b, 'e');
        break;
    }
}
/**
 * Создание и заполнение динамического буфера
 *
 */
uint8_t *bencode_encode(const ben_obj_t *obj, size_t *out_len) {
    dynbuf_t b;
    dynbuf_init(&b);
    encode_obj(&b, obj);
    if (out_len) *out_len = b.len;
    return b.data; // владение передаётся вызывающему
}

/**
 * Декодирование данных в формате bencode.
 *
 * @ *data - указатель на данные в формате bencode
 * @ *size - размер данных
 * @ ret - указатель на объект с данными ben_obj_t
  */
ben_obj_t *bencode_decode(const uint8_t *data, size_t size) {
    const uint8_t *end = data + size;
    ben_obj_t *obj = xmalloc(sizeof(ben_obj_t));
    const uint8_t *next = NULL;

    if (isdigit(*data)) {
        next = parse_string(data, end, obj);
    } else if (*data == 'i') {
        next = parse_int(data, end, obj);
    } else if (*data == 'l') {
        next = parse_list(data, end, obj);
    } else if (*data == 'd') {
        next = parse_dict(data, end, obj);
    } else {
        LOG_ERROR("Invalid bencode: unknown type '%c'", *data);
        free(obj);
        return NULL;
    }

    if (!next || next != end) {
        LOG_ERROR("Bencode parse error");
        bencode_free(obj);
        return NULL;
    }
    return obj;
}

/**
 * парсинг закодированной строки
 *
 * @*ptr - указатель на данные
 * @*end - конец данных
 * @*obj - указатель на заполняемый объект
 */
static const uint8_t *parse_string(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj) {
    /* пропускаем ":"
    * если двоеточия нет - это ошибка
    * за двоеточием - длина строки
    */
    const uint8_t *colon = memchr(ptr, ':', end - ptr);
    if (!colon) return NULL;
    long len = strtol((char*)ptr, NULL, 10);
    if (len < 0 || colon + 1 + len > end) return NULL;
    obj->type = BEN_STRING;
    obj->value.string.data = (uint8_t*)(colon + 1);
    obj->value.string.len = len;
    return colon + 1 + len;
}

/**
 * Парсинг закодированного числового значения
 *
 * @*ptr - указатель на данные
 * @*end - конец данных
 * @*obj - указатель на заполняемый объект
 */
static const uint8_t *parse_int(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj) {
    ptr++; // skip 'i'
           // ищем 'e', если нет - ошибка, выходим
    const uint8_t *e = memchr(ptr, 'e', end - ptr);
    if (!e) return NULL;
    char *endptr;
    obj->type = BEN_INT;
    obj->value.integer = strtol((char*)ptr, &endptr, 10);
    if (endptr != (char*)e) return NULL; // trailing chars
    // перемещаем указатель на символ после 'e'
    return e + 1;
}

/**
 * Парсинг списка
 *
 * @*ptr - указатель на данные
 * @*end - конец данных
 * @*obj - указатель на заполняемый объект
 */
static const uint8_t *parse_list(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj) {
    ptr++; // skip 'l'
    obj->type = BEN_LIST;
    obj->value.list.items = NULL;
    obj->value.list.count = 0;

    while (ptr < end && *ptr != 'e') {
        ben_obj_t *item = xmalloc(sizeof(ben_obj_t));
        const uint8_t *next = NULL;
        if (isdigit(*ptr)) next = parse_string(ptr, end, item);
        else if (*ptr == 'i') next = parse_int(ptr, end, item);
        else if (*ptr == 'l') next = parse_list(ptr, end, item);
        else if (*ptr == 'd') next = parse_dict(ptr, end, item);
        else {
            free(item);
            break;
        }
        if (!next) {
            free(item);
            break;
        }
        obj->value.list.count++;
        obj->value.list.items = xrealloc(obj->value.list.items,
                                        obj->value.list.count * sizeof(ben_obj_t));
        obj->value.list.items[obj->value.list.count-1] = *item; //копируем по значению
        free(item);
        ptr = next;
    }
    if (ptr >= end || *ptr != 'e') {
        // ошибка
        return NULL;
    }
    return ptr + 1;
}

/**
 * Парсинг словаря
 * @*ptr - указатель на данные
 * @*end - конец данных
 * @*obj - указатель на заполняемый объект
 */
static const uint8_t *parse_dict(const uint8_t *ptr, const uint8_t *end, ben_obj_t *obj) {
    ptr++; // skip 'd'
    obj->type = BEN_DICT;
    obj->value.dict.pairs = NULL;
    obj->value.dict.count = 0;

    while (ptr < end && *ptr != 'e') {
        // ключ — строка
        ben_obj_t key_obj;
        const uint8_t *next = parse_string(ptr, end, &key_obj);
        if (!next) break;
        char *key = xmalloc(key_obj.value.string.len + 1);
        memcpy(key, key_obj.value.string.data, key_obj.value.string.len);
        key[key_obj.value.string.len] = '\0';

        // значение — любой тип
        ben_obj_t *val_obj = xmalloc(sizeof(ben_obj_t));
        const uint8_t *val_next = NULL;
        if (isdigit(*next)) val_next = parse_string(next, end, val_obj);
        else if (*next == 'i') val_next = parse_int(next, end, val_obj);
        else if (*next == 'l') val_next = parse_list(next, end, val_obj);
        else if (*next == 'd') val_next = parse_dict(next, end, val_obj);
        else {
            free(key);
            free(val_obj);
            break;
        }
        if (!val_next) {
            free(key);
            free(val_obj);
            break;
        }

        obj->value.dict.count++;
        obj->value.dict.pairs = xrealloc(obj->value.dict.pairs,
                                        obj->value.dict.count * sizeof(ben_pair_t));
        ben_pair_t *pair = &obj->value.dict.pairs[obj->value.dict.count-1];
        pair->key = key; 
        pair->key_len = key_obj.value.string.len;
        pair->value = val_obj;

        ptr = val_next;
    }
    if (ptr >= end || *ptr != 'e') {
        return NULL;
    }
    return ptr + 1;
}

/**
 * Очистка объекта ben_obj_t
 * @obj - указатель на объект
 * @free_self - флаг, что нужно очисить список, вложенный в список...
 *
 */
static void bencode_free_internal(ben_obj_t *obj, int free_self) {
    if (!obj) return;
    switch (obj->type) {
        case BEN_STRING:
            break;
        case BEN_INT:
            break;
        case BEN_LIST:
            for (size_t i = 0; i < obj->value.list.count; i++) {
                bencode_free_internal(&obj->value.list.items[i], 0);
            }
            free(obj->value.list.items);
            obj->value.list.items = NULL;
            obj->value.list.count = 0;
            break;
        case BEN_DICT:
            for (size_t i = 0; i < obj->value.dict.count; i++) {
                free(obj->value.dict.pairs[i].key);
                bencode_free_internal(obj->value.dict.pairs[i].value, 1);
            }
            free(obj->value.dict.pairs);
            obj->value.dict.pairs = NULL;
            obj->value.dict.count = 0;
            break;
    }
    if (free_self) {
        xfree(obj);
    }
}

/**
 * Обертка для функции по очистке объекта benobj_t
 * 
 * @*obj - указатель на объект
 */
void bencode_free(ben_obj_t *obj) {
    bencode_free_internal(obj, 1);
}


/**
 * Функция находит и возвращает значение (benobj_t) по ключу
 * @*dict - указатель на объект, содержащий словарь
 * @*key - указатель на ключ
 * @return - benobj_t
 */
ben_obj_t *bencode_dict_get(const ben_obj_t *dict, const char *key) {
    if (!dict || dict->type != BEN_DICT) return NULL;
    for (size_t i = 0; i < dict->value.dict.count; i++) {
        if (strcmp(dict->value.dict.pairs[i].key, key) == 0)
            return dict->value.dict.pairs[i].value;
    }
    return NULL;
}

/**
 * Возвращает строку из объекта ben_obj_t
 * 
 * @*obj - указатель на объект со строкой
 * @*len - указатель на длину строки (возвращаемое значение)
 * @return - указатель на строку
 */
const uint8_t *bencode_string_data(const ben_obj_t *obj, size_t *len) {
    if (!obj || obj->type != BEN_STRING) return NULL;
    *len = obj->value.string.len;
    return obj->value.string.data;
}

/**
 * Возвращает значение типа int из объекта ben_onj_t
 * 
 * @*obj - указатель на объект, содержащий int
 * @return - значение типа int
 */
int64_t bencode_int_value(const ben_obj_t *obj) {
    if (!obj || obj->type != BEN_INT) return 0;
    return obj->value.integer;
}
