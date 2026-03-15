#include "peer.h"

/**
 * Выполняет handshake с пиром. Отправляет рукопожатие и проверяет ответ
 * @param sock номер сокета
 * @param *tor указатель на структуру с данными торрента (info_hash)
 * @param *my_peer_id указатель на наш peer_id
 * @param *peer_id_out укзатель на peer_id удаленного узла (заполняется после успешного handshake)
 * @return успех/ошибка (0/-1)
 */ 

int peer_handshake(int sock, const torrent_t *tor, const uint8_t *my_peer_id, uint8_t *peer_id_out) {
    uint8_t hs_out[HANDSHAKE_SIZE];
    uint8_t hs_in[HANDSHAKE_SIZE];

    memset(hs_out, 0, sizeof(hs_out));
    hs_out[0] = BT_PROTOCOL_LEN;
    memcpy(hs_out + 1, BT_PROTOCOL, BT_PROTOCOL_LEN);
    // 8 зарезервированных байт оставляем нулями (можно установить биты расширений позже)
    memcpy(hs_out + 28, tor->info_hash, 20);
    memcpy(hs_out + 48, my_peer_id, 20);

    if (send_full_timeout(sock, hs_out, HANDSHAKE_SIZE, HANDSHAKE_TIMEOUT) < 0) {
        LOG_ERROR("Failed to send handshake");
        goto handshake_error;
    }

    if (recv_full_timeout(sock, hs_in, HANDSHAKE_SIZE, HANDSHAKE_TIMEOUT) < 0) {
        LOG_ERROR("Failed to receive handshake");
        goto handshake_error;
    }

    // Проверяем протокол
    if (hs_in[0] != BT_PROTOCOL_LEN || memcmp(hs_in + 1, BT_PROTOCOL, BT_PROTOCOL_LEN) != 0) {
        LOG_ERROR("Invalid protocol in handshake");
        goto handshake_error;
    }


    // Проверяем info_hash
    if (memcmp(hs_in + 28, tor->info_hash, 20) != 0) {
        LOG_ERROR("Info hash mismatch in handshake");
        goto handshake_error;
    }

    if (peer_id_out) {
        memcpy(peer_id_out, hs_in + 48, 20);
    }

    return 0;
handshake_error:
    return -1;
}
/**
 * Отправляет пиру сообщение interested (ID 2), чтобы показать, что мы хотим скачивать. 
 * @param sock - сокет
 * @return успех/ошибка (0/-1)
 */
int peer_send_interested(int sock) {
    uint8_t msg[] = {0,0,0,1, 2}; // length=1, id=2 (interested)
    return send_full_timeout(sock, msg, 5, PEER_SEND_TIMEOUT);
}

/**
 * Запросить блок данных у пира. Сообщение request (ID 6).
 *
 * @param sock номер сокета
 * @param index индекс куска
 * @param begin смещение внутри куска в байтах
 * @param length длина блок (стандартно - 16KiB)
 * @return успех/ошибка (0/-1)
 */
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
    return send_full_timeout(sock, msg, 17, PEER_SEND_TIMEOUT);
}


/**
 * Прочитать следующее сообщение от пира. Возвращает идентификатор сообщения и динамически выделенный буфер с payload (включая данные после ID). 
 * При keep-alive (длина 0) возвращает msg_id = 0xFF и нулевой payload.
 *
 * @param sock номер сокета
 * @param *msg_id[out] идентификатор сообщения 
 * @param **payload полученные данные
 * @param *payload_len длина данных
 * @param timeout_ms таймаут
 * @return успех/ошибка (0/-1)
 */
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
        memcpy(*payload, msg + 1, *payload_len); //копируем данные, т.к. msg будет перезаписан в следующий раз
    } else {
        *payload = NULL;
    }
    free(msg);
    return 0;
}

/**
 * После отправки interested ожидать, пока пир не пришлёт unchoke (ID 1). 
 * За одно обрабатывает другие сообщения: bitfield, have, choke. 
 * Сохраняет битовое поле, если оно пришло.
 * 
 * @param sock номер сокета
 * @param expected_index индекс ожидаемого куска
 * @param *buffer 
 * @param length
 * @param timeout_ms
 * @return успех/ошибка (0/-1)
 */
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
            memcpy(&index, payload, 4); //индекс куска
            memcpy(&begin, payload + 4, 4); //позиция начала данных в куске
            index = ntohl(index);
            begin = ntohl(begin);
            size_t block_len = payload_len - 8;

            if (index != expected_index || begin != 0 || block_len != length) {
                // Не тот кусок или не с начала — мы запрашиваем кусок целиком с begin=0
                LOG_DEBUG("Ignored message id %d", msg_id);
                free(payload);
                continue; // игнорируем другие сообщения (можно обработать have и т.п.)
            }
            memcpy(buffer, payload + 8, block_len); //копируем данные в выходной буфер
            free(payload);
            return 0;// выходим, если получили запрашиваемый кусок данных
        } else {
            // Обрабатываем другие сообщения (choke, unchoke, have, bitfield) если нужно
            // Пока просто игнорируем
            LOG_DEBUG("Ignored message id %d", msg_id);
            free(payload);
        }
    }
}
/**
 * Функция ожидания unchoke с обработкой промежуточных сообщений (bitfield, have и т.д.)
 *
 * @param *peer указатель на струкутуру с данными о пире
 * @param timeout_ms таймаут
 * @return успех/ошибка (0/-1)
 */
int peer_wait_for_unchoke(peer_connection_t *peer, int timeout_ms) {
    while (running && peer->choked) {
        uint8_t msg_id;
        uint8_t *payload;
        size_t payload_len;
        int ret = peer_read_message(peer->sock, &msg_id, &payload, &payload_len, timeout_ms);
        if (ret < 0) return -1;
        if (msg_id == 0xFF) { // keep-alive
            free(payload);
            continue;
        }
        // Обработка различных сообщений
        switch (msg_id) {
        case 0: // choke
            peer->choked = 1;
            LOG_DEBUG("Received choke");
            break;
        case 1: // unchoke
            peer->choked = 0;
            LOG_DEBUG("Received unchoke");
            break;
        case 4: // have
            if (payload_len >= 4) {
                uint32_t index = ntohl(*(uint32_t*)payload);
                if (peer->bitfield) {
                    size_t byte = index / 8;
                    if (byte < peer->bitfield_len) {
                        peer->bitfield[byte] |= 1 << (7 - (index % 8));
                    } else {
                        // У пира появились новые данные, можно расширить битовое поле
                        LOG_DEBUG("RECIEVED \"HAVE\" for a piece %u beyond current bitfield", index);
                    }
                }
            }
         break;
        case 5: // bitfield
            // Сохраняем битовое поле (копируем)
            peer->bitfield = xmalloc(payload_len);
            memcpy(peer->bitfield, payload, payload_len);
            peer->bitfield_len = payload_len;
            LOG_DEBUG("Received bitfield (%zu bytes)", payload_len);
            break;
        default:
            LOG_DEBUG("Ignored message id %d", msg_id);
            break;
        }
        free(payload);
        if (!peer->choked) break; // выходим, если мы больше не chocked 
    }
    return peer->choked ? -1 : 0;
}

/**
 * Проверяет, есть ли у пира кусок с данным индексом. 
 * 
 * @param *peer указатель на структуру с данными о пире
 * @param index индекс куска данных
 * @return 1/0 есть/нет (бит из bitfield)
 */
int peer_has_piece(peer_connection_t *peer, uint32_t index) {
    if (!peer->bitfield) return 1; // нет информации - считаем, что есть
    size_t byte = index / 8;
    if (byte >= peer->bitfield_len) return 0;
    return (peer->bitfield[byte] >> (7 - (index % 8))) & 1;
}

/**
 * Ожидаетт и получает блок данных, соответствующий ранее отправленному request. 
 * Функция игнорирует сообщения других блоков и обрабатывает возможные choke.
 *
 * @param sock номер сокета
 * @param expected_index ожидаемый индекс куска
 * @param expected_begin ожидаемое начало куска (смещение)
 * @param *buffer указатель на буфер для записи данных
 * @param length ожидаема длина данных
 * @param timeout_ms таймаут
 * @return успех/ошибка (0/-1)
 */
int peer_receive_block(int sock, uint32_t expected_index, uint32_t expected_begin, uint8_t *buffer, size_t length, int timeout_ms) {
    uint8_t *payload;
    size_t payload_len;
    uint8_t msg_id;

    while (1) {
        if (peer_read_message(sock, &msg_id, &payload, &payload_len, timeout_ms) < 0) {
            LOG_ERROR("Peer read message failed...");
            return -1;
        }
        if (msg_id == 0xFF) { // keep-alive
            free(payload);
            continue;
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

            if (index == expected_index && begin == expected_begin && block_len == length) {
                memcpy(buffer, payload + 8, block_len);
                free(payload);
                return 0;
            } else {
                // Не тот блок — возможно, пришёл блок от другого запроса (если мы отправляли несколько)
                LOG_DEBUG("Ignored piece %u:%u (expected %u:%u)", index, begin, expected_index, expected_begin);
                free(payload);
                // Продолжаем ждать нужный
            }
        } else {
            LOG_DEBUG("Received msg_id=%d while waiting for block", msg_id);
            // Обрабатываем другие сообщения: choke, unchoke, have, keep-alive
            // Если получили choke — возможно, пир нас задушил, надо выйти с ошибкой
            if (msg_id == 0) { // choke
                LOG_DEBUG("Received choke while waiting for block");
                free(payload);
                return -1; // Пир задушил, прерываем
            }
            LOG_DEBUG("Ignored message id %d while waiting for block", msg_id);
            free(payload);
        }
    }
}

/**
 * Закрыть соединение с пиром
 * @param *peer указатель на структуру с данными о пире
 */
void peer_close(peer_connection_t *peer) {
    if (peer->sock >= 0) close(peer->sock);
    free(peer->bitfield);
    memset(peer, 0, sizeof(*peer));
}
