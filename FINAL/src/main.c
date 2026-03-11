#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "utils.h"
#include "torrent.h"
#include "peer.h"
#include "tracker.h"
#include "storage.h"
#include "network.h"
#include "tar.h"

/**
 * Зааполняет данные из торрента в структуру tor
 * @cfg - конфигурация (пути, контекст вывода и т.д.)
 * @tor - данные о торренте
 *
 */
int load_torrent(torrent_t *tor, config_t *cfg);

/**
 * Выводит загруженные данные
 * @tor - Торрент
 */
void log_info_about_torrent(torrent_t *tor);

/**
 * Инициализирует контекст вывода в зависимости от настроек.
 * @param cfg       Конфигурация
 * @param tor       Торрент (входной параметр, только для чтения)
 * @return 0 при успехе, -1 при ошибке (ресурсы не освобождаются, вызывающий должен сделать cleanup)
 */
static int setup_output_context(config_t *cfg, const torrent_t *tor); 

/**
 * Загружает все недостающие куски торрента, перебирая пиров по очереди.
 *
 * @param tor         Указатель на структуру торрента.
 * @param peers       Массив доступных пиров.
 * @param peer_count  Количество пиров в массиве.
 * @param my_peer_id  Наш идентификатор (20 байт).
 * @param cfg         Указатель на конфигурацию (содержит информацию о выводе).
 *
 * @return Количество оставшихся (нескачанных) кусков. 0, если все скачаны успешно.
 */
int download_pieces(const torrent_t *tor, const peer_t *peers, int peer_count, const uint8_t my_peer_id[20],const config_t *cfg);

int main(int argc, char **argv) {
    config_t cfg;
    torrent_t tor;

    parse_args(argc, argv, &cfg);
    setup_signals();

    if(load_torrent(&tor, &cfg)) {
        return 1;
    }

    log_info_about_torrent(&tor);

    uint8_t my_peer_id[20];
    generate_peer_id(my_peer_id);
    peer_t *peers = NULL;

    int peer_count = tracker_get_peers(&tor, my_peer_id, &peers);
    if (peer_count <= 0) {
        LOG_ERROR("No peers received from tracker");
        torrent_free(&tor);
        free_config(&cfg);
        return 1;
    }
    // Переключаем вывод: хранилище/архив
    if (setup_output_context(&cfg, &tor) != 0) {
        free(peers);
        torrent_free(&tor);
        free_config(&cfg);
        return 1;
    }
    int pieces_left = download_pieces(&tor, peers, peer_count, my_peer_id, &cfg);
    if (pieces_left == 0) {
        LOG_INFO("All pieces downloaded successfully!");
    } else {
        LOG_ERROR("Download incomplete, %d pieces missing", pieces_left);
    }

    free(peers);
    torrent_free(&tor);
    free_config(&cfg);
    return 0;
}

int load_torrent(torrent_t *tor, config_t *cfg) {
    if (cfg->use_stdin) {
        LOG_INFO("Loading torrent from stdin");
        uint8_t *data;
        size_t size = read_stdin(&data);
        if (size == 0) {
            LOG_ERROR("Failed to read torrent from stdin");
            goto load_error;
        }
        if (torrent_load_from_memory(data, size, tor) != 0) {
            LOG_ERROR("Failed to parse torrent from stdin");
            free(data);
            goto load_error;
        }
        free(data);
    } else if (cfg->input_file) {
        LOG_INFO("Loading torrent from %s", cfg->input_file);
        if (torrent_load(cfg->input_file, tor) != 0) {
            LOG_ERROR("Failed to load torrent");
            goto load_error;
        }
    } else if (cfg->watch_dir) {
        LOG_INFO("Watching directory %s (not implemented)", cfg->watch_dir);
        goto load_error;
    } else {
        LOG_ERROR("No input source specified");
        goto load_error;
    }
    return 0;
load_error:
    free_config(cfg);
    return 1;
}

void log_info_about_torrent(torrent_t *tor) {

// Выводим информацию
    LOG_INFO("Announce: %s", tor->announce ? tor->announce : "(none)");
    LOG_INFO("Name: %s", tor->name);
    LOG_INFO("Total length: %llu bytes", (unsigned long long)tor->total_length);
    LOG_INFO("Piece length: %u", tor->piece_length);
    LOG_INFO("Number of pieces: %u", tor->num_pieces);
    LOG_INFO("Number of files: %zu", tor->file_count);
#ifdef DEBUG
    for (size_t i = 0; i < tor->file_count; i++) {
        // Собираем путь
        fprintf(stderr,"[DEBUG]  File %zu: ", i);
        for (size_t j = 0; j < tor->files[i].path_len; j++) {
            fprintf(stderr,"%s/", tor->files[i].path[j]);
        }
        fprintf(stderr," (%llu bytes)\n", (unsigned long long)tor->files[i].length);
    }
#endif
}

/**
 * Инициализирует контекст вывода в зависимости от настроек.
 * @param cfg       Конфигурация
 * @param tor       Торрент (входной параметр, только для чтения)
 * @return 0 при успехе, -1 при ошибке (ресурсы не освобождаются, вызывающий должен сделать cleanup)
 */
static int setup_output_context(config_t *cfg, const torrent_t *tor) {
    if (cfg->output_file || cfg->extract_dir) {
        // Режим сохранения в файл/директорию
        if (cfg->output_file && tor->file_count > 1) {
            LOG_ERROR("Cannot use -o with multi-file torrent. Use -O for directory.");
            return -1;
        }
        storage_t *st = storage_open(cfg, tor);
        if (!st) {
            LOG_ERROR("Failed to open storage");
            return -1;
        }
        cfg->out_ctx = st;
        cfg->use_tar = 0;
    } else {
        // Режим tar-архива в stdout
        tar_writer_t *tw = tar_writer_open(stdout, tor);
        if (!tw) {
            LOG_ERROR("Failed to open tar writer");
            return -1;
        }
        cfg->out_ctx = tw;
        cfg->use_tar = 1;
    }
    return 0;
}

/**
 * Загружает все недостающие куски торрента, перебирая пиров по очереди.
 *
 * @param tor         Указатель на структуру торрента.
 * @param peers       Массив доступных пиров.
 * @param peer_count  Количество пиров в массиве.
 * @param my_peer_id  Наш идентификатор (20 байт).
 * @param cfg         Указатель на конфигурацию (содержит информацию о выводе).
 *
 * @return Количество оставшихся (нескачанных) кусков. 0, если все скачаны успешно.
 */
int download_pieces(const torrent_t *tor, const peer_t *peers, int peer_count,
                    const uint8_t my_peer_id[20],const config_t *cfg)

{
    // Массив для отметки скачанных кусков
    uint8_t *pieces_done = xcalloc((tor->num_pieces + 7) / 8, 1);
    int pieces_left = tor->num_pieces;

    for (int peer_idx = 0; peer_idx < peer_count && pieces_left > 0 && running; peer_idx++) {
        peer_t p = peers[peer_idx];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &p.ip, ip_str, sizeof(ip_str));
        uint16_t port = ntohs(p.port);
        LOG_INFO("Trying peer %s:%u (%d/%d)", ip_str, port, peer_idx + 1, peer_count);

        int sock = tcp_connect_timeout(p.ip, p.port, CONNECTIOIN_TIMEOUT);
        if (sock < 0) {
            LOG_WARN("Failed to connect to peer");
            continue;
        }

        peer_connection_t peer = {
            .sock = sock,
            .choked = 1,
            .bitfield = NULL,
            .bitfield_len = 0
        };

        uint8_t peer_id_resp[20];
        if (peer_handshake(sock, tor, my_peer_id, peer_id_resp) < 0) {
            LOG_WARN("Handshake failed");
            close(sock);
            continue;
        }

        LOG_INFO("Handshake successful with peer, waiting for unchoke...");
        if (peer_send_interested(sock) < 0) {
            LOG_WARN("Failed to send interested");
            close(sock);
            continue;
        }

        if (peer_wait_for_unchoke(&peer, UNCHOKE_TIMEOUT) < 0) {
            LOG_WARN("Failed to get unchoke");
            peer_close(&peer);
            continue;
        }

        // Получили unchoke, начинаем скачивать недостающие куски
        for (uint32_t i = 0; i < tor->num_pieces && pieces_left > 0 && running; i++) {
            if (IS_DONE(pieces_done, i)) continue; // уже скачан

            if (!peer_has_piece(&peer, i)) {
                LOG_DEBUG("Peer lacks piece %u", i);
                continue;
            }

            uint32_t piece_len = piece_size(tor, i);
            uint8_t *buf = xmalloc(piece_len);
            int piece_ok = 1;

            uint32_t offset = 0;
            while (offset < piece_len && piece_ok && running) {
                uint32_t block_len = (piece_len - offset) > BLOCK_SIZE ? BLOCK_SIZE : (piece_len - offset);
                if (peer_send_request(sock, i, offset, block_len) < 0) {
                    LOG_ERROR("Failed to send request for piece %u block %u", i, offset);
                    piece_ok = 0;
                    break;
                }
                if (peer_receive_block(sock, i, offset, buf + offset, block_len, RECEIVE_TIMEOUT) < 0) {
                    LOG_ERROR("Failed to receive block %u for piece %u", offset, i);
                    piece_ok = 0;
                    break;
                }
                offset += block_len;
            }

            if (piece_ok && verify_piece(tor, i, buf)) {
                // Записываем кусок в нужный обработчик
                if (cfg->use_tar) {
                    tar_writer_write((tar_writer_t*)cfg->out_ctx, i, buf, piece_len);
                } else {
                    storage_write((storage_t*)cfg->out_ctx, i, buf, piece_len);
                }
                MARK_DONE(pieces_done, i);
                pieces_left--;
                LOG_INFO("Piece %u done, %d left", i, pieces_left);
            } else {
                LOG_ERROR("Failed to download piece %u", i);
                // Не помечаем, попробуем у другого пира
            }
            free(buf);
        }

        peer_close(&peer);
    }

    // Закрываем контекст вывода
    if (cfg->use_tar) {
        tar_writer_close((tar_writer_t*)cfg->out_ctx);
    } else {
        storage_close((storage_t*)cfg->out_ctx);
    }


    free(pieces_done);
    return pieces_left;
}

