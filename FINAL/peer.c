#include "peer.h"
#include "network.h"
#include "utils.h"
#include <string.h>
#include <arpa/inet.h>

#define BT_PROTOCOL "BitTorrent protocol"
#define BT_PROTOCOL_LEN 19
#define HANDSHAKE_SIZE 68

int peer_handshake(int sock, const torrent_t *tor, uint8_t *peer_id_out) {
    uint8_t hs_out[HANDSHAKE_SIZE];
    uint8_t hs_in[HANDSHAKE_SIZE];

    memset(hs_out, 0, sizeof(hs_out));
    hs_out[0] = BT_PROTOCOL_LEN;
    memcpy(hs_out + 1, BT_PROTOCOL, BT_PROTOCOL_LEN);
    // 8 зарезервированных байт оставляем нулями (можно установить биты расширений позже)
    memcpy(hs_out + 28, tor->info_hash, 20);
    // peer_id: можно использовать случайный, но для теста возьмём фиксированный
    const char *test_peer_id = "-TU0001-123456789012";
    memcpy(hs_out + 48, test_peer_id, 20);

    if (send_full_timeout(sock, hs_out, HANDSHAKE_SIZE, 10000) < 0) {
        LOG_ERROR("Failed to send handshake");
        return -1;
    }

    if (recv_full_timeout(sock, hs_in, HANDSHAKE_SIZE, 10000) < 0) {
        LOG_ERROR("Failed to receive handshake");
        return -1;
    }

    // Проверяем протокол
    if (hs_in[0] != BT_PROTOCOL_LEN || memcmp(hs_in + 1, BT_PROTOCOL, BT_PROTOCOL_LEN) != 0) {
        LOG_ERROR("Invalid protocol in handshake");
        return -1;
    }

    // Проверяем info_hash
    if (memcmp(hs_in + 28, tor->info_hash, 20) != 0) {
        LOG_ERROR("Info hash mismatch in handshake");
        return -1;
    }

    if (peer_id_out) {
        memcpy(peer_id_out, hs_in + 48, 20);
    }

    return 0;
}

int peer_send_interested(int sock) {
    uint8_t msg[] = {0,0,0,1, 2}; // length=1, id=2 (interested)
    return send_full_timeout(sock, msg, 5, 5000);
}

int peer_send_request(int sock, uint32_t index, uint32_t begin, uint32_t length) {
    uint8_t msg[17];
    uint32_t len = htonl(13); // payload length (без length поля)
    memcpy(msg, &len, 4);
    msg[4] = 6; // request id
    uint32_t idx = htonl(index);
    memcpy(msg+5, &idx, 4);
    uint32_t beg = htonl(begin);
    memcpy(msg+9, &beg, 4);
    uint32_t l = htonl(length);
    memcpy(msg+13, &l, 4);
    return send_full_timeout(sock, msg, 17, 5000);
}

int peer_read_message(int sock, uint8_t *msg_id, uint8_t **payload, size_t *payload_len, int timeout_ms) {
    uint8_t *msg;
    size_t len;
    if (recv_message(sock, &msg, &len, timeout_ms) < 0) {
        return -1;
    }
    if (len == 0) {
        // keep-alive (нет ID)
        *msg_id = 0xFF; // специальное значение, означающее keep-alive
        *payload = NULL;
        *payload_len = 0;
        return 0;
    }
    *msg_id = msg[0];
    *payload_len = len - 1;
    if (*payload_len > 0) {
        *payload = xmalloc(*payload_len);
        memcpy(*payload, msg + 1, *payload_len);
    } else {
        *payload = NULL;
    }
    free(msg);
    return 0;
}

int peer_receive_piece(int sock, uint32_t expected_index, uint8_t *buffer, size_t length, int timeout_ms) {
    uint8_t *payload;
    size_t payload_len;
    uint8_t msg_id;

    while (1) {
        if (peer_read_message(sock, &msg_id, &payload, &payload_len, timeout_ms) < 0) {
            return -1;
        }
        if (msg_id == 7) { // piece
            if (payload_len < 8) {
                free(payload);
                return -1;
            }
            uint32_t index, begin;
            memcpy(&index, payload, 4);
            memcpy(&begin, payload + 4, 4);
            index = ntohl(index);
            begin = ntohl(begin);
            size_t block_len = payload_len - 8;

            if (index != expected_index || begin != 0 || block_len != length) {
                // Не тот кусок или не с начала — в нашей упрощённой схеме мы запрашиваем кусок целиком с begin=0
                free(payload);
                continue; // игнорируем другие сообщения (можно обработать have и т.п.)
            }
            memcpy(buffer, payload + 8, block_len);
            free(payload);
            return 0;
        } else {
            // Обрабатываем другие сообщения (choke, unchoke, have, bitfield) если нужно
            // Пока просто игнорируем
            LOG_DEBUG("Ignored message id %d", msg_id);
            free(payload);
        }
    }
}
