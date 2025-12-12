#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

extern uint32_t calculate_crc32c(uint32_t crc32c, const unsigned char *buffer,  unsigned int length);

int main(int argc, char *argv[]) {
    // Проверка аргументов
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <файл>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int fd = -1;
    struct stat st;
    uint32_t crc = 0;
    int success = 0; // Флаг успешного выполнения
    
    // Инициализация таблицы
    //init_crc32_table();

    // Открытие файла
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Ошибка открытия файла '%s': %s\n",
                filename, strerror(errno));
        return EXIT_FAILURE;
    }

    // Получение информации о файле
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "Ошибка получения информации о файле: %s\n",
                strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    // Проверка, что это обычный файл
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "'%s' не является обычным файлом\n", filename);
        close(fd);
        return EXIT_FAILURE;
    }

    size_t file_size = st.st_size;
    
    // Обработка пустого файла
    if (file_size == 0) {
        // CRC32 пустого файла
        printf("%08x\n", crc);
        close(fd);
        return EXIT_SUCCESS;
    }

    // Определение размера отображаемого блока
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) {
        page_size = 4096;
    }
    
    size_t chunk_size = 1024 * 1024; // 1 МБ
    if ((size_t)page_size > chunk_size) {
        chunk_size = page_size;
    }

    // Выравнивание chunk_size на границу страницы
    chunk_size = (chunk_size + page_size - 1) & ~(page_size - 1);

    // Обработка файла по частям
    size_t offset = 0;
    crc = 0xFFFFFFFF; // Начальное значение CRC
    
    while (offset < file_size) {
        size_t remaining = file_size - offset;
        size_t map_size = (remaining < chunk_size) ? remaining : chunk_size;

        // Отображение части файла
        void *mapped = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, offset);
        if (mapped == MAP_FAILED) {
            fprintf(stderr, "Ошибка mmap: %s\n", strerror(errno));
            success = 0;
            break;
        }

        // Вычисление CRC для текущего блока
        //extern uint32_t calculate_crc32c(uint32_t crc32c, const unsigned char *buffer,  unsigned int length);
        //crc = crc32_compute(mapped, map_size, crc);
        crc = calculate_crc32c(crc, mapped, map_size);

        // Освобождение отображения
        if (munmap(mapped, map_size) == -1) {
            fprintf(stderr, "Предупреждение: ошибка munmap: %s\n", 
                    strerror(errno));
            // Не прерываем выполнение, так как данные уже считаны
        }

        offset += map_size;
        success = 1; // Успешно обработан хотя бы один блок
    }

    // Закрытие файлового дескриптора
    if (close(fd) == -1) {
        fprintf(stderr, "Ошибка при закрытии файла: %s\n", strerror(errno));
        // Не возвращаем ошибку, так как CRC уже вычислен
    }

    // Проверка успешности выполнения
    if (!success && offset < file_size) {
        // Не удалось обработать весь файл
        return EXIT_FAILURE;
    }

    // Вывод результата
    printf("%08x\n", crc);
    
    return EXIT_SUCCESS;
}
