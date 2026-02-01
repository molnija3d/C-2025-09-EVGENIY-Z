#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

int main() {
    printf("=== FTRACE SYSCALL DEMO ===\n");
    printf("PID: %d\n", getpid());
    printf("\nCOPY THE PID NUMBER ABOVE!\n");
    printf("Set up ftrace in another terminal, then press Enter...\n");
    printf("Waiting for Enter: ");
    fflush(stdout);
    
    getchar();  // Wait for user to set up ftrace
    
    printf("\n=== SYSCALL OPERATIONS START ===\n");
    
    // SYSCALL 1: openat() - open/create file
    printf("1. Opening file (openat syscall)...\n");
    int fd = open("/tmp/test_ftrace.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        exit(1);
    }
    printf("   File opened, fd = %d\n", fd);
    
    // SYSCALL 2: write() - write data to file
    printf("2. Writing to file (write syscall)...\n");
    const char *message = "Hello from ftrace syscall demo!\n";
    ssize_t bytes_written = write(fd, message, strlen(message));
    printf("   Written %ld bytes\n", bytes_written);
    
    // SYSCALL 3: newfstat() - get file information
    printf("3. Getting file info (newfstat syscall)...\n");
    struct stat file_stat;
    if (fstat(fd, &file_stat) == 0) {
        printf("   File size: %ld bytes\n", file_stat.st_size);
        printf("   File permissions: %o\n", file_stat.st_mode & 0777);
    }
    
    // SYSCALL 4: close() - close file descriptor
    printf("4. Closing file (close syscall)...\n");
    close(fd);
    printf("   File closed\n");
    
    // SYSCALL 5: unlink() - delete file
    printf("5. Deleting file (unlink syscall)...\n");
    if (unlink("/tmp/test_ftrace.txt") == 0) {
        printf("   File deleted\n");
    } else {
        perror("unlink failed");
    }
    
    printf("\n=== SYSCALL OPERATIONS END ===\n");
    printf("Check /tmp/ftrace_output.txt for syscall traces!\n");
    
    return 0;
}

