CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -lpthread -lc
LDFLAGS =

all: parser

daemon: parser.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o parser parser.c

clean:
	rm -f *.o parser 

