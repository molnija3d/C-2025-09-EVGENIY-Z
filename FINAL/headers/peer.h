#ifndef PEER_H
#define PEER_H

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "torrent.h"
#include "network.h"
#include "utils.h"

#define BLOCK_SIZE 16384  // 16 KiB
#define BT_PROTOCOL "BitTorrent protocol"
#define BT_PROTOCOL_LEN 19
#define HANDSHAKE_SIZE 68
#define HANDSHAKE_TIMEOUT 10000
#define PEER_SEND_TIMEOUT 5000
#define PEER_ID_LEN 20
                          
typedef struct {
    uint32_t ip;   // в сетевом порядке (big-endian)
    uint16_t port; // в сетевом порядке
} peer_t;


typedef struct {
    int sock;
    uint8_t *bitfield;
    size_t bitfield_len;
    int choked;
    int interested;
}peer_connection_t ;

// Проверить, есть ли у пира кусок с данным индексом
int peer_has_piece(peer_connection_t *peer, uint32_t index);

// Выполнить handshake с пиром
// Возвращает 0 при успехе, -1 при ошибке
int peer_handshake(int sock, const torrent_t *tor, const uint8_t *my_peer_id, uint8_t *peer_id_out);

// Отправить сообщение interested
int peer_send_interested(int sock);

// Отправить сообщение request
int peer_send_request(int sock, uint32_t index, uint32_t begin, uint32_t length);

// Прочитать и обработать сообщение от пира (низкоуровневая функция)
// Возвращает -1 при ошибке, 0 при успехе, *msg_id заполняется (0..255)
int peer_read_message(int sock, uint8_t *msg_id, uint8_t **payload, size_t *payload_len, int timeout_ms);

// Получить кусок (после отправки request ожидает piece сообщение)
int peer_receive_piece(int sock, uint32_t expected_index, uint8_t *buffer, size_t length, int timeout_ms);

// Ждем unchoke
int peer_wait_for_unchoke(peer_connection_t *peer, int timeout_ms);

// Освобождение ресурсов пира
void peer_close(peer_connection_t *peer);

//Получение блока
int peer_receive_block(int sock, uint32_t expected_index, uint32_t expected_begin,
                       uint8_t *buffer, size_t length, int timeout_ms);
#endif
