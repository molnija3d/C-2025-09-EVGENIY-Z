CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g 
LDFLAGS =

all: webserver

webserver: webserver.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o webserver	webserver.c

clean:
	rm -f *.o webserver	

