CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
LDFLAGS =

all: check_crc32

check_crc32: check_crc32.c crc32.o 
	$(CC) $(CFLAGS) $(LDFLAGS) -o check_crc32 check_crc32.c crc32.o
crc32.0: crc32.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c crc32.c 

clean:
	rm -f *.o crc32*

