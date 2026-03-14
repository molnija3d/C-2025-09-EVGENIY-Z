#include "network.h"

/**
 * Устанавливает tcp-соединение с таймаутом
 * 
 * @param ip адрес
 * @param port порт
 * @param timeout_ms таймаут
 * @return дескриптор сокета в блокирующем режиме
 */
int tcp_connect_timeout(uint32_t ip, uint16_t port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // Устанавливаем неблокирующий режим для connect с таймаутом
    int flags = fcntl(sock, F_GETFL, 0); // Получаем флаги
    if (flags == -1) {
        perror("fcntl F_GETFL");
        close(sock);
        return -1;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) { //устанавливаем флаги, переводим в неблокирующий режим
        perror("fcntl F_SETFL O_NONBLOCK");
        close(sock);
        return -1;
    }
    /* Инициализация адреса */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port; 
    addr.sin_addr.s_addr = ip;

    int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(sock);
        return -1;
    }

    if (ret == 0) {
        // Соединение установилось мгновенно
        // Возвращаем сокет в блокирующий режим
        fcntl(sock, F_SETFL, flags);
        return sock;
    }

    // Ждём завершения соединения с помощью poll
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLOUT;

    int poll_ret = poll(&pfd, 1, timeout_ms);
    if (poll_ret <= 0) {
        if (poll_ret == 0) {
            LOG_ERROR("Connection timeout to %s:%d", inet_ntoa(addr.sin_addr), ntohs(port));
        } else {
            perror("poll");
        }
        close(sock);
        return -1;
    }

    // Проверяем, есть ли ошибка на сокете
    int so_error;
    socklen_t len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
        perror("getsockopt");
        close(sock);
        return -1;
    }
    if (so_error != 0) {
        LOG_ERROR("Connection error: %s", strerror(so_error));
        close(sock);
        return -1;
    }

    /* Возвращаем сокет в блокирующий режим
       Перевод в неблокирующий режим только для connect необходим, 
       так как стандартный блокирующий connect может висеть минутами. 
       После установки соединения сокет возвращается в блокирующий режим, 
       чтобы упростить последующие операции (не нужно обрабатывать EAGAIN).*/
     
    fcntl(sock, F_SETFL, flags);
    return sock;
}


/**
 * Отправляет len байт из буфера c заданным таймаутом
 *
 * @param sock номер сокета
 * @param *buf указатель на буфер с данными
 * @param len длина данных
 * @param timeout_ms таймаут
 * @return успех/ошибка (0/1)
 */
int send_full_timeout(int sock, const void *buf, size_t len, int timeout_ms) {
    size_t sent = 0; // счетчик отправленных байт
    while (sent < len && running) {
        struct pollfd pfd = { .fd = sock, .events = POLLOUT };
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            return -1;
        }
        if (ret == 0) {
            LOG_ERROR("Send timeout");
            return -1;
        }
        ssize_t n = send(sock, (const char*)buf + sent, len - sent, 0);
        if (n <= 0) {
            if (n == 0) {
                LOG_ERROR("Connection closed during send");
            } else {
                perror("send");
            }
            return -1;
        }
        sent += n;
    }
    return running ? 0 : -1; //если цикл завершен из-за running (устанваливается в обработчике сигналов), возвращаем -1
}

/**
 * Получает len байт из сокета в буфер с таймаутом
 *
 * @param sock номер сокета
 * @param *buf указатель на буфер с данными
 * @param len длина данных
 * @param timeout_ms таймаут
 * @return успех/ошибка (0/1)
 */
int recv_full_timeout(int sock, void *buf, size_t len, int timeout_ms) {
    size_t received = 0; //счетчик полученных байт
    while (received < len && running) {
        struct pollfd pfd = { .fd = sock, .events = POLLIN };
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            return -1;
        }
        if (ret == 0) {
            LOG_ERROR("Receive timeout");
            return -1;
        }
        ssize_t n = recv(sock, (char*)buf + received, len - received, 0);
        if (n <= 0) {
            if (n == 0) {
                LOG_ERROR("Connection closed during recv");
            } else {
                perror("recv");
            }
            return -1;
        }
        received += n;
    }
    return running ? 0 : -1;
}

/**
 * Читает сообщение протокола BitTorrent, которое состоит из 4-байтового префикса длины (в сетевом порядке) и следующих за ним данных (включая идентификатор сообщения).
 * Возвращает динамически выделенный буфер с payload (все байты после префикса длины) и его размер.
 * payload содержит первым байтом идентификатор сообщения (например, 7 для piece)
 * 
 * @param sock номер сокета
 * @param **payload указатель на динамический буфер
 * @param *payload_len указатель на длину сообщения
 * @param timeout_ms таймаут
 * @return
 */
int recv_message(int sock, uint8_t **payload, size_t *payload_len, int timeout_ms) {
    uint32_t len_prefix; //длина сообщения
    if (recv_full_timeout(sock, &len_prefix, 4, timeout_ms) < 0) {
            LOG_ERROR("Receive len failed");
        return -1;
    }
    uint32_t msg_len = ntohl(len_prefix); //т.к. в сетевом порядке
    if (msg_len == 0) {
        // keep-alive сообщение 
        *payload = NULL;
        *payload_len = 0;
        return 0;
    }
    *payload = xmalloc(msg_len); // выделяем память и читаем сообщение
    if (recv_full_timeout(sock, *payload, msg_len, timeout_ms) < 0) {
            LOG_ERROR("Receive message failed");
        free(*payload);
        return -1;
    }
    *payload_len = msg_len;
    return 0;
}
