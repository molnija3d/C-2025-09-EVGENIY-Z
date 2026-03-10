#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bits/getopt_core.h>
#include "config.h"
#include "utils.h"
#include "torrent.h"
#include "peer.h"
#include "tracker.h"
#include "storage.h"
#include "network.h"
#include "tar.h"
#define IS_DONE(pieces, idx) ((pieces)[(idx)/8] & (1 << (7 - ((idx)%8))))
#define MARK_DONE(pieces, idx) ((pieces)[(idx)/8]  |= (1<< (7 - ((idx)%8))))

void parse_args(int argc, char **argv, config_t *cfg) {
    memset(cfg, 0, sizeof(config_t));
    cfg->use_stdin = 1;
    cfg->use_stdout = 1;
    int opt;
    while ((opt = getopt(argc, argv, "f:d:o:O:")) != -1) {
        switch (opt) {
        case 'f':
            cfg->input_file = strdup(optarg);
            cfg->use_stdin = 0;
            break;
        case 'd':
            cfg->watch_dir = strdup(optarg);
            cfg->use_stdin = 0;
            break;
        case 'o':
            cfg->output_file = strdup(optarg);
            cfg->use_stdout = 0;
            break;
        case 'O':
            cfg->extract_dir = strdup(optarg);
            cfg->use_stdout = 0;
            break;
        default:
            LOG_ERROR("Usage: %s [-f file.torrent | -d dir] [-o file | -O dir]\n", argv[0]);
            exit(1);
        }
    }
}

void free_config(config_t *cfg) {
    free(cfg->input_file);
    free(cfg->watch_dir);
    free(cfg->output_file);
    free(cfg->extract_dir);
}

int main(int argc, char **argv) {
    config_t cfg;
    parse_args(argc, argv, &cfg);
    setup_signals();

    torrent_t tor;

    if (cfg.use_stdin) {
        uint8_t *data;
        size_t size = read_stdin(&data);
        if (size == 0) {
            LOG_ERROR("Failed to read torrent from stdin");
            free_config(&cfg);
            return 1;
        }
        if (torrent_load_from_memory(data, size, &tor) != 0) {
            LOG_ERROR("Failed to parse torrent from stdin");
            free(data);
            free_config(&cfg);
            return 1;
        }
        free(data);
    } else if (cfg.input_file) {
        LOG_INFO("Loading torrent from %s", cfg.input_file);
        if (torrent_load(cfg.input_file, &tor) != 0) {
            LOG_ERROR("Failed to load torrent");
            free_config(&cfg);
            return 1;
        }
    } else if (cfg.watch_dir) {
        LOG_INFO("Watching directory %s (not implemented)", cfg.watch_dir);
        free_config(&cfg);
        return 1;
    } else {
        LOG_ERROR("No input source specified");
        free_config(&cfg);
        return 1;
    }

// Выводим информацию
    LOG_INFO("Announce: %s", tor.announce ? tor.announce : "(none)");
    LOG_INFO("Name: %s", tor.name);
    LOG_INFO("Total length: %llu bytes", (unsigned long long)tor.total_length);
    LOG_INFO("Piece length: %u", tor.piece_length);
    LOG_INFO("Number of pieces: %u", tor.num_pieces);
    LOG_INFO("Number of files: %zu", tor.file_count);
#ifdef DEBUG
    for (size_t i = 0; i < tor.file_count; i++) {
        // Собираем путь
        fprintf(stderr,"[DEBUG]  File %zu: ", i);
        for (size_t j = 0; j < tor.files[i].path_len; j++) {
            fprintf(stderr,"%s/", tor.files[i].path[j]);
        }
        fprintf(stderr," (%llu bytes)\n", (unsigned long long)tor.files[i].length);
    }
#endif
    
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
    // Тип вывода хранилище/архив
    void *out_ctx = NULL;
    int use_tar = 0;  // 1 - tar_writer, 0 - storage
    if (cfg.output_file || cfg.extract_dir) {
        // Режим сохранения в файл/директорию
        if (cfg.output_file && tor.file_count > 1) {
            LOG_ERROR("Cannot use -o with multi-file torrent. Use -O for directory.");
            free(peers);
            torrent_free(&tor);
            free_config(&cfg);
            return 1;
        }
        // Открываем хранилишще

        storage_t *st = storage_open(&cfg, &tor);
        if (!st) {
            LOG_ERROR("Failed to open storage");
            free(peers);
            torrent_free(&tor);
            free_config(&cfg);
            return 1;
        }
        out_ctx = st;
    } else {
        // Режим tar-архива в stdout
        tar_writer_t *tw = tar_writer_open(stdout, &tor);
        if (!tw) {
            LOG_ERROR("Failed to open tar writer");
            free(peers);
            torrent_free(&tor);
            free_config(&cfg);
            return 1;
        }
        out_ctx = tw;
        use_tar = 1;
    }

    // Массив для отметки скачанных кусков
    uint8_t *pieces_done = xcalloc((tor.num_pieces + 7) / 8, 1);
    int pieces_left = tor.num_pieces;


    for (int peer_idx = 0; peer_idx < peer_count && pieces_left > 0 && running; peer_idx++) {
        peer_t p = peers[peer_idx];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &p.ip, ip_str, sizeof(ip_str));
        uint16_t port = ntohs(p.port);
        LOG_INFO("Trying peer %s:%u (%d/%d)", ip_str, port, peer_idx+1, peer_count);

        int sock = tcp_connect_timeout(p.ip, p.port, 10000);
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
        if (peer_handshake(sock, &tor, my_peer_id, peer_id_resp) < 0) {
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

        if (peer_wait_for_unchoke(&peer, 30000) < 0) {
            LOG_WARN("Failed to get unchoke");
            peer_close(&peer);
            continue;
        }

        // Получили unchoke, считаем, что есть bitfield, можно начинать загрузку недостающих кусков
        // начинаем скачивать недостающие куски
        for (uint32_t i = 0; i < tor.num_pieces && pieces_left > 0 && running; i++) {
            if (IS_DONE(pieces_done, i)) continue; // уже скачан

            if (!peer_has_piece(&peer, i)) {
                LOG_DEBUG("Peer lacks piece %u", i);
                continue;
            }

            uint32_t piece_len = piece_size(&tor, i);
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
                if (peer_receive_block(sock, i, offset, buf + offset, block_len, 30000) < 0) {
                    LOG_ERROR("Failed to receive block %u for piece %u", offset, i);
                    piece_ok = 0;
                    break;
                }
                offset += block_len;
            }

            if (piece_ok && verify_piece(&tor, i, buf)) {
                // Записываем кусок в нужный обработчик
                if (use_tar) {
                    tar_writer_write((tar_writer_t*)out_ctx, i, buf, piece_len);
                } else {
                    storage_write((storage_t*)out_ctx, i, buf, piece_len);
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

    if (use_tar) {
        tar_writer_close((tar_writer_t*)out_ctx);
    } else {
        storage_close((storage_t*)out_ctx);
    }

    if (pieces_left == 0) {
        LOG_INFO("All pieces downloaded successfully!");
    } else {
        LOG_ERROR("Download incomplete, %d pieces missing", pieces_left);
    }

    free(pieces_done);
    free(peers);
    torrent_free(&tor);
    free_config(&cfg);
    return 0;
}
