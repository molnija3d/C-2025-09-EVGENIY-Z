#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include "ring_buffer_ioctl.h"

int main(int argc, char *argv[]) {
    int fd;
    ssize_t ret;
    char buffer[1024];
    struct pollfd pfd;
    
    printf("Тестирование чтения из кольцевого буфера\n");
    
    // Открываем устройство
    fd = open("/dev/ringbuffer", O_RDONLY);
    if (fd < 0) {
        perror("Не удалось открыть устройство");
        return 1;
    }
    
    // Настраиваем poll для ожидания данных
    pfd.fd = fd;
    pfd.events = POLLIN;
    
    while (1) {
        printf("Ожидание данных...\n");
        
        // Ждем данные с таймаутом 5 секунд
        ret = poll(&pfd, 1, 5000);
        if (ret < 0) {
            perror("Ошибка poll");
            break;
        } else if (ret == 0) {
            printf("Таймаут ожидания данных\n");
            break;
        }
        
        if (pfd.revents & POLLIN) {
            // Читаем данные
            ret = read(fd, buffer, sizeof(buffer) - 1);
            if (ret < 0) {
                perror("Ошибка чтения");
                break;
            } else if (ret == 0) {
                printf("Буфер пуст\n");
            } else {
                buffer[ret] = '\0';
                printf("Прочитано %zd байт:\n%s\n", ret, buffer);
            }
        }
    }
    
    close(fd);
    return 0;
}
