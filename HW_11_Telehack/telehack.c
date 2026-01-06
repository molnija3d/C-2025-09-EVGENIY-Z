#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>

#define TELNET_PORT 23
#define BUFFER_SIZE 4096
#define TIMEOUT_SEC 2
#define TIMEOUT_USEC 0
#define MAX_READ 1500
#define MAX_COMMAND_LENGTH 256

/* В случае, когда весь арт получен*/
#define MORE_FULL "--More--(100"

/* Эту надпись нужно искать, чтобы понять влез весь текст в экрн или нет*/
#define MORE_PATTERN "--More--"
/* Паттерн для проскроливания ASCII арт, если он не влазит в экран*/
#define MORE_COMMAND "\r\n"   

// Чтение данных из сокета с таймаутом
static int read_with_timeout(int sock_fd, char *buffer, size_t size) {
    fd_set read_fds;
    struct timeval tv;

    FD_ZERO(&read_fds);
    FD_SET(sock_fd, &read_fds);

    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = TIMEOUT_USEC;

    int ready = select(sock_fd + 1, &read_fds, NULL, NULL, &tv);
    if (0 > ready) {
            /*  Select error */
        return -1; 
    } else if (0 == ready) {
            /* Timeout */
        return 0; 
    } else {
        return recv(sock_fd, buffer, size - 1, 0);
    }
    return -1;
}

/* Подключение к серверу
*/
  static int connect_to_server(const char *hostname, int port) {
    /* Получение информации о хосте */
    struct hostent *server_addr;
    server_addr = gethostbyname(hostname);
    if (!server_addr) {
        fprintf(stderr, "Ошибка: не удалось разрешить hostname '%s'\n", hostname);
        return -1;
    }

    /* Создание сокета */
    int sock_fd;
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > sock_fd) {
        perror("Ошибка создания сокета");
        return -1;
    }

    /* Настройка адреса сервера */
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);

    if (server_addr->h_addr_list[0]) {
        memcpy(&sock_addr.sin_addr.s_addr, server_addr->h_addr_list[0], server_addr->h_length);
    } else {
        fprintf(stderr, "Ошибка: нет адресов для хоста '%s'\n", hostname);
        close(sock_fd);
        return -1;
    }


    /* Установка таймаута на подключение */
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = TIMEOUT_USEC;

    /* Установка таймаутов на операции с сокетом */
    if (0 > setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        perror("Предупреждение: не удалось установить таймаут на чтение");
    }

    if (0 > setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) {
        perror("Предупреждение: не удалось установить таймаут на запись");
    }

    /* Подключение */
    if (0 > connect(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr))) {
        perror("Ошибка подключения к серверу");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

/*  Обработка приветствия сервера (ждем приглашение) */
static int skip_banner(int sock_fd) {
    char buffer[BUFFER_SIZE];
    int total_read = 0;
    int bytes_read = 0;

    /* Читаем данные пока не найдем приглашение (точку) */
    while (0 < (bytes_read = read_with_timeout(sock_fd, buffer, BUFFER_SIZE))) {
        buffer[bytes_read] = '\0';
        total_read += bytes_read;

        /* Ищем точку (приглашение telehack) */
            if (strstr(buffer,"\n\r.")) {
                return 0;
            }

        /* Защита от бесконечного цикла */
        if (total_read > MAX_READ) {
            fprintf(stderr, "Предупреждение: слишком длинный баннер, пропускаем\n");
            break;
        }
    }

    if (0 > bytes_read) {
        perror("Ошибка чтения баннера");
    }

    return 0;
}

/* Отправка команды figlet и получение результата */
static int send_figlet_command(int sock_fd, const char *font, const char *text) {
    char command[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    int bytes_read = 0, command_length = 0;
    int result_printed = 0;

    /* Формирование команды */
    snprintf(command, sizeof(command), "figlet /%s %s\r\n", font, text);
    command_length = strlen(command);
    if (command_length > MAX_COMMAND_LENGTH){
            /* Если длина команды больше, чем может принять сервис Telehack, 
             * обрезаем команду до макисмально возомжной длины */
            command_length = MAX_COMMAND_LENGTH;
    }
    /* Отправка команды */
    if (0 > send(sock_fd, command, strlen(command), 0) ) {
        perror("Ошибка отправки команды");
        return -1;
    }

    /* Чтение ответа */
    while (0 < (bytes_read = read_with_timeout(sock_fd, buffer, BUFFER_SIZE))) {
        char *start_ptr = 0;
        char *more_ptr = 0;
        buffer[bytes_read] = '\0';
        /* указатель на начало строки для вывода в консоль */
        start_ptr = buffer;
        /* указатель на строку --More-- */
        more_ptr = strstr(buffer, MORE_PATTERN);

        if(strstr(buffer, MORE_FULL)) {
        /* если достигли 100% - прерываем чтение ответа*/
            break;
        }

        /* удаляем посланную команду из ответа сервера */
        if(command_length && bytes_read > command_length) {
            start_ptr = &buffer[command_length];
            command_length = 0;
        }
        else if (command_length) {
            *start_ptr = '\0'; 
            command_length -= bytes_read;
        }

        if(NULL == more_ptr) {
        /* пока не найден --More--, выводим данные из буфера*/
            printf("%s", start_ptr);
            result_printed = 1;
        }
        else {
            printf("%s", start_ptr);
            /* если встретили --More-- -отправляем серверу команду на получение новых данных (MORE_COMMAND == "\r\n") */
            if(send(sock_fd, MORE_COMMAND, strlen(MORE_COMMAND), 0) < 0) {
                perror("Ошибка отправки команды");
            }
        }
    }

    printf("\r\n");
    if (0 > bytes_read) {
        perror("Ошибка чтения ответа");
        return -1;
    }

    if (!result_printed) {
        fprintf(stderr, "Не удалось получить ASCII-арт\n");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    /* Проверка аргументов командной строки */
    if (3 != argc) {
        fprintf(stderr, "Использование: %s <шрифт> <текст>\n", argv[0]);
        fprintf(stderr, "Пример: %s standard \"Hello World\"\n", argv[0]);
        fprintf(stderr, "Доступные шрифты: standard, banner, big, script, hollywood, starwars, и т.д.\n");
        return EXIT_FAILURE;
    }

    const char *font = argv[1];
    const char *text = argv[2];
    int sock_fd = -1;

    /* Подключение к серверу */
    sock_fd = connect_to_server("telehack.com", TELNET_PORT);
    if (0 > sock_fd) {
        return EXIT_FAILURE;
    }

    /* Пропускаем приветственный баннер */
    if (0 > skip_banner(sock_fd)) {
        close(sock_fd);
        return EXIT_FAILURE;
    }

    /* Отправляем команду figlet и получаем результат */
    int result = send_figlet_command(sock_fd, font, text);

    /* Закрытие соединения */
    close(sock_fd);

    return 0 == result ? EXIT_SUCCESS : EXIT_FAILURE;
}
