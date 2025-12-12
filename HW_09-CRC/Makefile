CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
LDFLAGS =

all: crc32

crc32: crc32.c crc32.h 
	$(CC) $(CFLAGS) $(LDFLAGS) -o crc32 crc32.c 

clean:
	rm -f *.o fs-daemon 

