#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    printf("=== Демонстрация системных вызовов ===\n");
    
    // 1. write - вывод в консоль
    write(STDOUT_FILENO, "1. Системный вызов write()\n", 27);
    
    // 2. open + close - работа с файлом
    int fd = open("test_file.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        printf("2. Файл создан через open()\n");
        close(fd);
    }
    
    // 3. stat - получение информации о файле
    struct stat file_stat;
    if (stat("syscall_demo.c", &file_stat) == 0) {
        printf("3. Получена информация о файле через stat(), размер: %ld байт\n", 
               file_stat.st_size);
    }
    
    // 4. fork + wait - создание процесса
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        printf("4. Дочерний процесс создан через fork()\n");
        exit(0);
    } else if (pid > 0) {
        wait(NULL); // Ожидание завершения дочернего процесса
        printf("4. Родительский процесс после wait()\n");
    }
    
    // 5. getpid + getppid - получение идентификаторов
    printf("5. PID процесса: %d, PPID: %d\n", getpid(), getppid());
    
    // 6. read - чтение с stdin
    char buffer[100];
    printf("Введите текст для демонстрации read(): ");
    fflush(stdout);
    read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    printf("Прочитано через read(): %.*s", 50, buffer);
    
    // 7. unlink - удаление файла
    unlink("test_file.txt");
    printf("7. Файл удален через unlink()\n");
    
    printf("=== Программа завершена ===\n");
    return 0;
}