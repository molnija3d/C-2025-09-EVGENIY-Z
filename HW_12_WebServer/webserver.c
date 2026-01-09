#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define HTTP_HEADER_SIZE 8192
#define MAX_PATH_LEN 4096
#define METHOD_LEN 16
#define PATH_LEN 1024
#define PROTOCOL_LEN 16
#define MAX_HTML_LEN (1024*1024)

typedef struct {
    char method[METHOD_LEN];
    char path[PATH_LEN];
    char protocol[PROTOCOL_LEN];
} http_request_t;

typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    size_t buffer_len;
    int response_sent;
} client_data_t;

/* Парсинг HTTP-запроса */
static int parse_http_request(const char* request, http_request_t* req) {
    char method[METHOD_LEN], path[PATH_LEN], protocol[PROTOCOL_LEN];

    if (sscanf(request, "%15s %1023s %15s", method, path, protocol) != 3) {
        return -1;
    }

    strncpy(req->method, method, sizeof(req->method) - 1);
    strncpy(req->path, path, sizeof(req->path) - 1);
    strncpy(req->protocol, protocol, sizeof(req->protocol) - 1);

    return 0;
}

/* Проверка безопасности пути (предотвращение path traversal) */
static int is_safe_path(const char* base_dir, const char* requested_path) {
    char* real_base = realpath(base_dir, NULL);
    if (real_base == NULL) {
        return 0;
    }

    /* Строим полный путь запроса */
    char full_requested_path[MAX_PATH_LEN * 2];
    snprintf(full_requested_path, sizeof(full_requested_path), "%s/%s", base_dir, requested_path);

    char* real_requested = realpath(full_requested_path, NULL);
    if (real_requested == NULL) {
        free(real_base);
        return 0;
    }

    /* Проверяем, что запрошенный путь находится внутри базовой директории */
    size_t base_len = strlen(real_base);
    int is_safe = (strncmp(real_base, real_requested, base_len) == 0);

    free(real_base);
    free(real_requested);

    return is_safe;
}

/* Получение MIME-типа по расширению файла */
static const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 || strcmp(ext, ".conf") == 0 || strcmp(ext, ".h") == 0)
        return "text/plain";
    if (strcmp(ext, ".json") == 0)
        return "application/json";
    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";

    return "application/octet-stream";
}

/* Генерация HTML-списка файлов */
static int generate_file_list(const char* dir_path, char* output, size_t max_size) {
    DIR* dir = opendir(dir_path);
    if (!dir) return -1;

    struct dirent* entry;
    struct stat file_stat;
    char time_buf[64];

    /* Начало HTML-документа */
    char* ptr = output;
    int remaining = max_size;

    /* Формируем шапку страницы */
    int written = snprintf(ptr, remaining,
                           "<!DOCTYPE html>\n"
                           "<html>\n"
                           "<head>\n"
                           "    <title>Directory Listing</title>\n"
                           "    <style>\n"
                           "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
                           "        h1 { color: #333; }\n"
                           "        table { border-collapse: collapse; width: 100%%; }\n"
                           "        th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }\n"
                           "        tr:hover { background-color: #f5f5f5; }\n"
                           "        a { text-decoration: none; color: #0066cc; }\n"
                           "        a:hover { text-decoration: underline; }\n"
                           "        .size { text-align: right; }\n"
                           "        .dir { font-weight: bold; }\n"
                           "    </style>\n"
                           "</head>\n"
                           "<body>\n"
                           "    <h1>Directory Listing: %s</h1>\n"
                           "    <table>\n"
                           "        <tr>\n"
                           "            <th>Name</th>\n"
                           "            <th>Size</th>\n"
                           "            <th>Modified</th>\n"
                           "        </tr>\n"
                           "        <tr>\n"
                           "            <td><a href=\"../\">..</a></td>\n"
                           "            <td class=\"size\">-</td>\n"
                           "            <td>-</td>\n"
                           "        </tr>\n",
                           dir_path);

    /* если шапка занимает больше памяти, чем отведено, закрываем директорию, возвращаем ошибку */
    if (written >= remaining) {
        closedir(dir);
        return -1;
    }

    ptr += written;
    remaining -= written;

    /* Запись каждого элемента директории */
    while ((entry = readdir(dir)) != NULL) {
         /* Пропускаем . и .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (stat(full_path, &file_stat) == -1) {
            continue;
        }

        struct tm* tm_info = localtime(&file_stat.st_mtime);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

        const char* class_name = S_ISDIR(file_stat.st_mode) ? "dir" : "";
        char size_formatted[32] = "-";

        /* если это файл - определяем размер */
        if (!S_ISDIR(file_stat.st_mode)) {
            if (file_stat.st_size < 1024) {
                snprintf(size_formatted, sizeof(size_formatted), "%ld B", file_stat.st_size);
            } else if (file_stat.st_size < 1024 * 1024) {
                snprintf(size_formatted, sizeof(size_formatted), "%.1f KB",
                         file_stat.st_size / 1024.0);
            } else {
                snprintf(size_formatted, sizeof(size_formatted), "%.1f MB",
                         file_stat.st_size / (1024.0 * 1024.0));
            }
        }

        /* добавляем в ответ блок с данными о файле или папке */
        written = snprintf(ptr, remaining,
                           "        <tr>\n"
                           "            <td class=\"%s\"><a href=\"%s%s\">%s</a></td>\n"
                           "            <td class=\"size\">%s</td>\n"
                           "            <td>%s</td>\n"
                           "        </tr>\n",
                           class_name,
                           entry->d_name,
                           S_ISDIR(file_stat.st_mode) ? "/" : "",
                           entry->d_name,
                           size_formatted,
                           time_buf);

        /* если записали в ответ больше, чем разрешено - прерываем дальнейшую обработку */
        if (written >= remaining) break;
        ptr += written;
        remaining -= written;
    }

    closedir(dir);

    /* Завершение HTML-документа */
    written = snprintf(ptr, remaining,
                       "    </table>\n"
                       "</body>\n"
                       "</html>\n");

    /* если записали в ответ больше данных, чем допустимо - возвращаем ошибку*/
    if (written >= remaining) return -1;

    return 0;
}

/* Отправка HTTP-ответа */
static void send_http_response(int client_fd, int status_code, const char* status_text, const char* content_type, const char* body, size_t body_len) {
    char header[HTTP_HEADER_SIZE];
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    char date_buf[64];

    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    /* заполняем заголовок */
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Date: %s\r\n"
                              "Server: Simple HTTP Server\r\n"
                              "Connection: close\r\n"
                              "Content-Length: %zu\r\n",
                              status_code, status_text,
                              date_buf,
                              body_len);

    /* если есть content_type, добавляем */
    if (content_type) {
        header_len += snprintf(header + header_len, sizeof(header) - header_len, 
                              "Content-Type: %s\r\n", content_type);
    }

    /* добавляем пустую строку */
    header_len += snprintf(header + header_len, sizeof(header) - header_len, "\r\n");

    /* Отправляем заголовок */
    send(client_fd, header, header_len, 0);

    /*  Отправляем тело, если есть */
    if (body && body_len > 0) {
        send(client_fd, body, body_len, 0);
    }
}

/* Установка сокета в неблокирующий режим */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
            return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Обработка HTTP-запроса */
static void handle_http_request(int client_fd, const char* request, const char* base_dir) {
    http_request_t req;

    /* обработка запроса (метод, путь, протокол ) */
    if (parse_http_request(request, &req) == -1) {
        const char* bad_request = "400 Bad Request";
        /* если нудачно, отправляем ошибку обработки запроса */
        send_http_response(client_fd, 400, bad_request, "text/html", bad_request, strlen(bad_request));
        return;
    }

    /* Поддерживаем только GET */
    if (strcmp(req.method, "GET") != 0) {
        /* уведомляем, что метод не поддерживается */
        const char* not_allowed = "405 Method Not Allowed";
        send_http_response(client_fd, 405, not_allowed, "text/html", not_allowed, strlen(not_allowed));
        return;
    }

    /* Обрабатываем путь */
    char file_path[MAX_PATH_LEN * 2];
    /* если запрашиваемый путь - корень, то используем директорию, переданную в параметрах в качестве пути */
    if (strcmp(req.path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s", base_dir);
    } else {
        /* если отличается - добавляем путь к дирректории */
        snprintf(file_path, sizeof(file_path), "%s%s", base_dir, req.path);
    }

    /* Проверяем безопасность пути */
    if (!is_safe_path(base_dir, req.path)) {
        const char* forbidden = "403 Forbidden";
        send_http_response(client_fd, 403, forbidden, "text/html", forbidden, strlen(forbidden));
        return;
    }

    struct stat file_stat;

    /* Проверяем существование файла/директории */
    if (stat(file_path, &file_stat) == -1) {
        const char* not_found = "404 Not Found";
        send_http_response(client_fd, 404, not_found, "text/html", not_found, strlen(not_found));
        return;
    }

    /* Проверяем права доступа */
    if (access(file_path, R_OK) == -1) {
        const char* forbidden = "403 Forbidden";
        send_http_response(client_fd, 403, forbidden, "text/html", forbidden, strlen(forbidden));
        return;
    }

    /* Если это директория - показываем список файлов */
    if (S_ISDIR(file_stat.st_mode)) { 
        /* для списка файлов */
        char* html_content = malloc(MAX_HTML_LEN);
        if (!html_content) {
            /* не удадлось выделить память - возвращаем внтреннюю ошибку сервера */
            const char* server_error = "500 Internal Server Error";
            send_http_response(client_fd, 500, server_error, "text/html", server_error, strlen(server_error));
            return;
        }

        if (generate_file_list(file_path, html_content, MAX_HTML_LEN) == 0) {
            /* если список сгенерирован успешно - возвращаем 200 и содержимое в html формате */
            send_http_response(client_fd, 200, "OK", "text/html", html_content, strlen(html_content));
        } else {
            /* в противном случае - внутреняя ошибка сервера */
            const char* server_error = "500 Internal Server Error";
            send_http_response(client_fd, 500, server_error, "text/html", server_error, strlen(server_error));
        }

        free(html_content);
        return;
    }

    /* если запрошен файл (запрошенный путь совпадает с путем файла) - отправляем его содержимое */
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        /* если файл не обнаружен, возвращаем 404 */
        const char* not_found = "404 Not Found";
        send_http_response(client_fd, 404, not_found, "text/html", not_found, strlen(not_found));
        return;
    }



    /* поределяем mime-type для файла*/
    const char* mime_type = get_mime_type(file_path);
    
    /* Заполняем структуру времени */
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    char date_buf[64];
    
    /* Конвертируем время в строку */
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", tm_info);

    /* Формируем заголовок */
    char header[HTTP_HEADER_SIZE];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Date: %s\r\n"
                              "Server: Simple HTTP Server\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              date_buf,
                              mime_type,
                              file_stat.st_size);

    /* Отправляем заголовок */
    send(client_fd, header, header_len, 0);

    /* Отправляем содержимое файла с помощью sendfile */
    off_t offset = 0;
    while (offset < file_stat.st_size) {
        ssize_t sent = sendfile(client_fd, file_fd, &offset, file_stat.st_size - offset);
        if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }
    }

    close(file_fd);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <host:port>\n", argv[0]);
        return 1;
    }

    const char* base_dir = argv[1];
    const char* host_port = argv[2];

    /* Парсим хост и порт */
    char host[256] = "0.0.0.0";
    int port = 8080;

    char host_copy[256];
    /* Делаем копию переменной окружения */
    strncpy(host_copy, host_port, sizeof(host_copy) - 1);
    host_copy[sizeof(host_copy) - 1] = '\0';

    char* colon = strchr(host_copy, ':');
    /* Если есть разделитель, заполняем и хост и порт */
    if (colon) {
        *colon = '\0';
        strncpy(host, host_copy, sizeof(host) - 1);
        port = atoi(colon + 1);
    } else {
        /* Если указан только порт - заполняем только порт */
        port = atoi(host_copy);
    }

    /* Проверяем существование базовой директории */
    struct stat dir_stat;
    if (stat(base_dir, &dir_stat) == -1 || !S_ISDIR(dir_stat.st_mode)) {
        fprintf(stderr, "Invalid directory: %s\n", base_dir);
        return 1;
    }

    /* Создаем сокет */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    /* Устанавливаем сокет в неблокирующий режим */
    if (set_nonblocking(server_fd) == -1) {
        perror("fcntl nonblocking");
        close(server_fd);
        return 1;
    }

    /* Устанавливаем опцию SO_REUSEADDR */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    /* Настраиваем адрес */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    /* заполняем структуру */
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        if (strcmp(host, "0.0.0.0") == 0) {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            perror("inet_pton");
            close(server_fd);
            return 1;
        }
    }

    /*  Привязываем сокет */
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* Начинаем слушать */
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server started on %s:%d\n", host, port);
    printf("Serving directory: %s\n", base_dir);

    /* Создаем epoll */
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    /* Добавляем серверный сокет в epoll */
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_fd);
        return 1;
    }

    /* Основной цикл событий */
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; i++) {
            /* Если data.fd == server_fd, то у нас новое подключение */
            if (events[i].data.fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

                if (client_fd == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept");
                    }
                    continue;
                }

                /* Устанавливаем неблокирующий режим для клиентского сокета */
                if (set_nonblocking(client_fd) == -1) {
                    perror("fcntl client nonblocking");
                    close(client_fd);
                    continue;
                }

                printf("New connection from %s:%d\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));

                /* Выделяем память для данных клиента */
                client_data_t* client_data = malloc(sizeof(client_data_t));
                if (!client_data) {
                    close(client_fd);
                    continue;
                }

                memset(client_data, 0, sizeof(client_data_t));
                client_data->fd = client_fd;

                /* Добавляем клиента в epoll */
                event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                event.data.ptr = client_data;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl client");
                    free(client_data);
                    close(client_fd);
                }
            } else {
                /* пришли данные от клиента */
                client_data_t* client_data = events[i].data.ptr;

                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    /* Соединение закрыто или ошибка */
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_data->fd, NULL);
                    close(client_data->fd);
                    free(client_data);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    /* Читаем данные */
                    ssize_t bytes_read;
                    while ((bytes_read = recv(client_data->fd, client_data->buffer + client_data->buffer_len, sizeof(client_data->buffer) - client_data->buffer_len - 1, 0)) > 0) {
                        client_data->buffer_len += bytes_read;

                        /* Проверяем конец HTTP-запроса (двойной CRLF) */
                        if (client_data->buffer_len >= 4) {
                            char* end = strstr(client_data->buffer, "\r\n\r\n");
                            if (end) {
                                /* Нуль-терминируем для безопасности */
                                size_t request_len = end - client_data->buffer + 4;
                                if (request_len < sizeof(client_data->buffer)) {
                                    client_data->buffer[request_len] = '\0';
                                }

                                /* Обрабатываем запрос */
                                handle_http_request(client_data->fd, client_data->buffer, base_dir);

                                /* Помечаем, что ответ отправлен */
                                client_data->response_sent = 1;

                                /* Закрываем соединение после отправки ответа */
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_data->fd, NULL);
                                close(client_data->fd);
                                free(client_data);
                                break;
                            }
                        }

                        /* Если буфер заполнен, но конец запроса не найден */
                        if (client_data->buffer_len >= sizeof(client_data->buffer) - 1) {
                            /* Отправляем ошибку */
                            const char* bad_request = "400 Bad Request";
                            send_http_response(client_data->fd, 400, bad_request,
                                               "text/html", bad_request, strlen(bad_request));

                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_data->fd, NULL);
                            close(client_data->fd);
                            free(client_data);
                            break;
                        }
                    }

                    /* Если соединение закрыто или ошибка */
                    if (bytes_read == 0 || (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        if (!client_data->response_sent) {
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_data->fd, NULL);
                            close(client_data->fd);
                            free(client_data);
                        }
                    }
                }
            }
        }
    }

    /* Очистка */
    close(server_fd);
    close(epoll_fd);

    return 0;
}
