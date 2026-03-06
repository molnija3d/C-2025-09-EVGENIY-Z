// peer.h
#ifndef PEER_H
#define PEER_H

#include <stdint.h>

typedef struct {
    uint32_t ip;   // в сетевом порядке (big-endian)
    uint16_t port; // в сетевом порядке
} peer_t;

#endif
