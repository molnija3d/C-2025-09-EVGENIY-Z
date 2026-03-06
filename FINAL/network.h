#ifndef NETWORK_H
#define NETWORK_H

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "utils.h"

// Подключение к пиру по IP и порту с таймаутом (в миллисекундах)
// Возвращает сокет или -1 при ошибке
int tcp_connect_timeout(uint32_t ip, uint16_t port, int timeout_ms);

// Отправка ровно len байт с таймаутом (возвращает 0 при успехе, -1 при ошибке)
int send_full_timeout(int sock, const void *buf, size_t len, int timeout_ms);

// Получение ровно len байт с таймаутом (возвращает 0 при успехе, -1 при ошибке)
int recv_full_timeout(int sock, void *buf, size_t len, int timeout_ms);

// Получение сообщения переменной длины: сначала читается 4-байтный length prefix,
// затем читается payload указанной длины (включая message ID)
// Возвращает 0 при успехе, -1 при ошибке; *payload выделяется динамически и должен быть освобождён вызывающим.
int recv_message(int sock, uint8_t **payload, size_t *payload_len, int timeout_ms);

#endif
