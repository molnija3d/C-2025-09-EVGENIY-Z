#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "ring_buffer_ioctl.h"

int main(int argc, char *argv[]) {
    int fd;
    ssize_t ret;
    char buffer[1024];
    int i;
    
    printf("Тестирование записи в кольцевой буфер\n");
    
    // Открываем устройство
    fd = open("/dev/ringbuffer", O_WRONLY);
    if (fd < 0) {
        perror("Не удалось открыть устройство");
        return 1;
    }
    
    // Записываем несколько сообщений
    for (i = 0; i < 10; i++) {
        snprintf(buffer, sizeof(buffer), 
                "Сообщение %d: Привет из пользовательского пространства!\n", i+1);
        
        ret = write(fd, buffer, strlen(buffer));
        if (ret < 0) {
            perror("Ошибка записи");
        } else {
            printf("Записано %zd байт: %s", ret, buffer);
        }
        
        sleep(1); // Задержка для демонстрации
    }
    
    close(fd);
    return 0;
}
