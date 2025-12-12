#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

// Таблица CRC32 (полином 0xEDB88320)
static uint32_t crc32_table[256];

// Инициализация таблицы CRC32
static void init_crc32_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
        }
        crc32_table[i] = c;
    }
}

// Вычисление CRC32 для блока данных
static uint32_t crc32_compute(const void *data, size_t len, uint32_t crc) {
    const uint8_t *bytes = (const uint8_t *)data;
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ bytes[i]) & 0xFF];
    }
    return ~crc;
}

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
    init_crc32_table();

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
        crc = crc32_compute(mapped, map_size, crc);

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