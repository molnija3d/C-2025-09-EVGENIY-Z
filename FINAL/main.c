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
            fprintf(stderr, "Usage: %s [-f file.torrent | -d dir] [-o file | -O dir]\n", argv[0]);
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
        // ... реализация чтения из stdin (пропустим для краткости)
        LOG_ERROR("stdin input not implemented yet");
        free_config(&cfg);
        return 1;
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
    for (size_t i = 0; i < tor.file_count; i++) {
        // Собираем путь
        printf("  File %zu: ", i);
        for (size_t j = 0; j < tor.files[i].path_len; j++) {
            printf("%s/", tor.files[i].path[j]);
        }
        printf(" (%llu bytes)\n", (unsigned long long)tor.files[i].length);
    }

    peer_t *peers = NULL;
    int peer_count = tracker_get_peers(&tor, &peers);
    if (peer_count > 0) {
        LOG_INFO("Received %d peers from tracker", peer_count);
        for (int i = 0; i < peer_count; i++) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peers[i].ip, ip_str, sizeof(ip_str));
            uint16_t port = ntohs(peers[i].port);
            LOG_INFO("Peer %d: %s:%u", i, ip_str, port);
        }
        free(peers);
    } else {
        LOG_ERROR("No peers received or error");
    }
    // Освобождение
    torrent_free(&tor);
    free_config(&cfg);
    return 0;
}
