#ifndef TRACKER_H
#define TRACKER_H

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <stdint.h> 
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include "torrent.h"
#include "peer.h"
#include "bencode.h"
#include "utils.h"

// Получить список пиров от трекера
// Возвращает количество пиров (или -1 при ошибке)
//int tracker_get_peers(const torrent_t *tor, peer_t **peers_out);
int tracker_get_peers(const torrent_t *tor, const uint8_t *peer_id, peer_t **peers_out);

// Генерация случайного peer_id (20 байт в виде строки)
void generate_peer_id(uint8_t *peer_id);

// Преобразование 20-байтного значения (info_hash, peer_id) в URL-encoded строку
char *url_encode(const uint8_t *hash);
//void ip_int32_to_string(uint32_t ip_net, char *buffer);
#endif
