#define _POSIX_C_SOURCE 200809L
#ifndef TRACKER_H
#define TRACKER_H

#include <arpa/inet.h>
#include "torrent.h"
#include "peer.h"

// Получить список пиров от трекера
// Возвращает количество пиров (или -1 при ошибке)
int tracker_get_peers(const torrent_t *tor, peer_t **peers_out);

#endif
