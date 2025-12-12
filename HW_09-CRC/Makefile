CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
LDFLAGS =

all: check_crc32

check_crc32: check_crc32.c crc32.c 
	$(CC) $(CFLAGS) $(LDFLAGS) -o check_crc32 check_crc32.c crc32.c

clean:
	rm -f *.o crc32*

