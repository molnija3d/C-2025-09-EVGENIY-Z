#include "network.h"

int tcp_connect_timeout(uint32_t ip, uint16_t port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // Устанавливаем неблокирующий режим для connect с таймаутом
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        close(sock);
        return -1;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port; // уже в сетевом порядке
    addr.sin_addr.s_addr = ip;

    int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(sock);
        return -1;
    }

    if (ret == 0) {
        // Соединение установилось мгновенно (редко)
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

    // Возвращаем сокет в блокирующий режим
    fcntl(sock, F_SETFL, flags);
    return sock;
}

int send_full_timeout(int sock, const void *buf, size_t len, int timeout_ms) {
    size_t sent = 0;
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
    return running ? 0 : -1;
}

int recv_full_timeout(int sock, void *buf, size_t len, int timeout_ms) {
    size_t received = 0;
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

int recv_message(int sock, uint8_t **payload, size_t *payload_len, int timeout_ms) {
    uint32_t len_prefix;
    if (recv_full_timeout(sock, &len_prefix, 4, timeout_ms) < 0) {
            LOG_ERROR("Recieve err 1");
        return -1;
    }
    uint32_t msg_len = ntohl(len_prefix);
    if (msg_len == 0) {
        // keep-alive сообщение (не должно встречаться, но обработаем)
        *payload = NULL;
        *payload_len = 0;
        return 0;
    }
    *payload = xmalloc(msg_len);
    if (recv_full_timeout(sock, *payload, msg_len, timeout_ms) < 0) {
            LOG_ERROR("Recieve err 2");
        free(*payload);
        return -1;
    }
    *payload_len = msg_len;
    return 0;
}
