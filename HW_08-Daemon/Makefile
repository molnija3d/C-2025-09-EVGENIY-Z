CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
LDFLAGS =

all: daemon

daemon: fs-daemon.c fs-daemon.h 
	$(CC) $(CFLAGS) $(LDFLAGS) -o fs-daemon fs-daemon.c 

clean:
	rm -f *.o fs-daemon 

