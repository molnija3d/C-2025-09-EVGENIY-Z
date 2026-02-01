CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g 
LDFLAGS =

all: main

demo: ftrace_demo.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o ftrace_demo	ftrace_demo.c

main: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o main	main.c

clean:
	rm -f *.o main ftrace_demo	

