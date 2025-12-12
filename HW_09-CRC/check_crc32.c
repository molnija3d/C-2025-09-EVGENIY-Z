#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>



extern uint32_t update_crc32(uint32_t crc, const void *buf, size_t size);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <файл>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int fd = -1;
    struct stat st;
    uint32_t crc = 0;
    int success = 0;
    
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Ошибка открытия файла '%s': %s\n",
                filename, strerror(errno));
        return EXIT_FAILURE;
    }

    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "Ошибка получения информации о файле: %s\n",
                strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "'%s' не является обычным файлом\n", filename);
        close(fd);
        return EXIT_FAILURE;
    }

    size_t file_size = st.st_size;
    
    if (file_size == 0) {
        printf("%08x\n", crc);
        close(fd);
        return EXIT_SUCCESS;
    }

    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) {
        page_size = 4096;
    }
    
    size_t chunk_size = 1024 * 1024; 
    if ((size_t)page_size > chunk_size) {
        chunk_size = page_size;
    }

    chunk_size = (chunk_size + page_size - 1) & ~(page_size - 1);

    size_t offset = 0;
    crc = 0xFFFFFFFF; 
    
    while (offset < file_size) {
        size_t remaining = file_size - offset;
        size_t map_size = (remaining < chunk_size) ? remaining : chunk_size;

        void *mapped = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, offset);
        if (mapped == MAP_FAILED) {
            fprintf(stderr, "Ошибка mmap: %s\n", strerror(errno));
            success = 0;
            break;
        }

        crc = update_crc32(crc, mapped, map_size);

        if (munmap(mapped, map_size) == -1) {
            fprintf(stderr, "Предупреждение: ошибка munmap: %s\n", 
                    strerror(errno));
        }

        offset += map_size;
        success = 1; 
    }

    if (close(fd) == -1) {
        fprintf(stderr, "Ошибка при закрытии файла: %s\n", strerror(errno));
    }

    if (!success && offset < file_size) {
        return EXIT_FAILURE;
    }

    crc ^= 0xFFFFFFFF;
    printf("%08x\n", crc);
    
    return EXIT_SUCCESS;
}
