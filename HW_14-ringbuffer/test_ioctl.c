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
    size_t size, used, free_space;
    struct rb_stats stats;
    int blocking = 1;
    
    printf("Тестирование ioctl команд кольцевого буфера\n");
    
    // Открываем устройство
    fd = open("/dev/ringbuffer", O_RDWR);
    if (fd < 0) {
        perror("Не удалось открыть устройство");
        return 1;
    }
    
    // 1. Получаем текущий размер
    if (ioctl(fd, RB_GET_SIZE, &size) < 0) {
        perror("RB_GET_SIZE failed");
    } else {
        printf("1. Текущий размер буфера: %zu байт\n", size);
    }
    
    // 2. Получаем статистику использования
    if (ioctl(fd, RB_GET_USED, &used) < 0) {
        perror("RB_GET_USED failed");
    } else {
        printf("2. Использовано: %zu байт\n", used);
    }
    
    if (ioctl(fd, RB_GET_FREE, &free_space) < 0) {
        perror("RB_GET_FREE failed");
    } else {
        printf("3. Свободно: %zu байт\n", free_space);
    }
    
    // 3. Изменяем размер буфера
    size = 8192;
    printf("4. Изменяем размер на %zu байт...\n", size);
    if (ioctl(fd, RB_SET_SIZE, &size) < 0) {
        perror("RB_SET_SIZE failed");
    } else {
        printf("   Размер успешно изменен\n");
    }
    
    // 4. Получаем полную статистику
    if (ioctl(fd, RB_GET_STATS, &stats) < 0) {
        perror("RB_GET_STATS failed");
    } else {
        printf("5. Полная статистика:\n");
        printf("   Размер: %zu байт\n", stats.size);
        printf("   Использовано: %zu байт\n", stats.used);
        printf("   Свободно: %zu байт\n", stats.free);
        printf("   Операций записи: %lu\n", stats.writes);
        printf("   Операций чтения: %lu\n", stats.reads);
        printf("   Переполнений: %lu\n", stats.overruns);
    }
    
    // 5. Переключаем в неблокирующий режим
    blocking = 0;
    printf("6. Переключаем в неблокирующий режим...\n");
    if (ioctl(fd, RB_SET_BLOCKING, &blocking) < 0) {
        perror("RB_SET_BLOCKING failed");
    }
    
    // 6. Очищаем буфер
    printf("7. Очищаем буфер...\n");
    if (ioctl(fd, RB_CLEAR, 0) < 0) {
        perror("RB_CLEAR failed");
    } else {
        printf("   Буфер очищен\n");
    }
    
    close(fd);
    return 0;
}
