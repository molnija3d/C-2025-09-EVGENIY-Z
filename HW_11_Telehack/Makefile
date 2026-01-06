CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g 
LDFLAGS =

all: telehack

telehack: telehack.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o telehack telehack.c

clean:
	rm -f *.o telehack 

