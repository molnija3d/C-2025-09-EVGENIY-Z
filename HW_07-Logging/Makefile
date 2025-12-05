CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
LDFLAGS = -rdynamic  # For backtrace 

all: test

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) -c logger.c

test: test.c logger.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o test test.c logger.o

clean:
	rm -f *.o example application.log

run: test
	./test
	cat application.log
