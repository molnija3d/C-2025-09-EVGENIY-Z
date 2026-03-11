#include "tracker.h"

// Структура для накопления данных ответа
struct memory {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        LOG_ERROR("Not enough memory for tracker response");
        return 0;
    }
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

// Преобразование 20-байтного значения (info_hash, peer_id) в URL-encoded строку
char *url_encode(const uint8_t *hash) {
    char *encoded = malloc(3*20 + 1); // каждый байт кодируется как %XX + нуль
    if (!encoded) return NULL;
    char *p = encoded;
    for (int i = 0; i < 20; i++) {
        sprintf(p, "%%%02X", hash[i]);
        p += 3;
    }
    *p = '\0';
    return encoded;
}

// Генерация случайного peer_id (20 байт в виде строки)
void generate_peer_id(uint8_t *peer_id) {
    srand(time(NULL));
    memcpy(peer_id, PEER_PREFIX , sizeof(PEER_PREFIX));
    for (int i = 8; i < 20; i++) {
        peer_id[i] = '0' + (rand() % 10);
    }
    peer_id[20] = '\0';
    LOG_DEBUG("Client_peer_id: %s", peer_id);
}

int tracker_get_peers(const torrent_t *tor, const uint8_t *peer_id, peer_t **peers_out) {
    CURL *curl;
    CURLcode res;
    struct memory chunk = { NULL, 0 };
    int peer_count = -1;
    *peers_out = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize curl");
        return -1;
    }

    // Формируем URL
    char *info_hash_enc = url_encode(tor->info_hash);
    if (!info_hash_enc) {
        free(info_hash_enc);
        curl_easy_cleanup(curl);
        return -1;
    }

    // Формируем peer_id
    char *peer_id_enc = url_encode(peer_id);
    if (!peer_id_enc) {
        free(peer_id_enc);
        curl_easy_cleanup(curl);
        return -1;
    }
    char url[2048];

    snprintf(url, sizeof(url),
             "%s?info_hash=%s&peer_id=%s&port=60703&uploaded=0&downloaded=0&left=%llu&compact=1&event=started",
             tor->announce ? tor->announce : "",
             info_hash_enc,
             peer_id_enc,
             (unsigned long long)tor->total_length);


    LOG_DEBUG("Tracker URL: %s", url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "qBittorrent/4.3.9");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // таймаут 30 секунд

    // Выполняем запрос
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG_ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
        goto cleanup;
    }

    // Проверяем HTTP код ответа
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        LOG_ERROR("Tracker returned HTTP %ld", http_code);
        goto cleanup;
    }

    // Парсим bencoded ответ
    ben_obj_t *resp = bencode_decode((uint8_t*)chunk.data, chunk.size);
    if (!resp || resp->type != BEN_DICT) {
        LOG_ERROR("Failed to parse tracker response");
        goto cleanup_bencode;
    }

    // Извлекаем peers
    ben_obj_t *peers_obj = bencode_dict_get(resp, "peers");
    if (!peers_obj || peers_obj->type != BEN_STRING) {
        LOG_ERROR("No peers field in tracker response or not a string");
        goto cleanup_bencode;
    }

    size_t peers_len;
    const uint8_t *peers_data = bencode_string_data(peers_obj, &peers_len);
    if (peers_len % 6 != 0) {
        LOG_ERROR("Invalid peers length: %zu (should be multiple of 6)", peers_len);
        goto cleanup_bencode;
    }

    peer_count = peers_len / 6;
    *peers_out = xmalloc(peer_count * sizeof(peer_t));

    for (int i = 0; i < peer_count; i++) {
        memcpy(&(*peers_out)[i].ip, peers_data + i*6, 4);
        memcpy(&(*peers_out)[i].port, peers_data + i*6 + 4, 2);
    }

cleanup_bencode:
    bencode_free(resp);

cleanup:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    free(chunk.data);
    free(info_hash_enc);
    free(peer_id_enc);
    return peer_count;
}
