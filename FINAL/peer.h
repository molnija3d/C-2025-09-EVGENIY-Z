#define _POSIX_C_SOURCE 200809L
#ifndef PEER_H
#define PEER_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "torrent.h"

typedef struct {
    uint32_t ip;   // в сетевом порядке (big-endian)
    uint16_t port; // в сетевом порядке
} peer_t;

// Состояние пира (может быть расширено позже)
typedef struct {
    int sock;
    uint8_t *bitfield;
    size_t bitfield_len;
    int am_choked;        // 1, если нас задушили
    int am_interested;    // 1, если мы заинтересованы
    int peer_choking;     // 1, если пир нас душит (обычно начинаем с 1)
    int peer_interested;  // 1, если пир заинтересован в нас
} peer_connection_t;

// Выполнить handshake с пиром
// Возвращает 0 при успехе, -1 при ошибке
int peer_handshake(int sock, const torrent_t *tor, uint8_t *peer_id_out);

// Отправить сообщение interested
int peer_send_interested(int sock);

// Отправить сообщение request
int peer_send_request(int sock, uint32_t index, uint32_t begin, uint32_t length);

// Прочитать и обработать сообщение от пира (низкоуровневая функция)
// Возвращает -1 при ошибке, 0 при успехе, *msg_id заполняется (0..255)
int peer_read_message(int sock, uint8_t *msg_id, uint8_t **payload, size_t *payload_len, int timeout_ms);

// Получить кусок (после отправки request ожидает piece сообщение)
int peer_receive_piece(int sock, uint32_t expected_index, uint8_t *buffer, size_t length, int timeout_ms);

// Освобождение ресурсов пира
void peer_close(peer_connection_t *peer);
#endif
