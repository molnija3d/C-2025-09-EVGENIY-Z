CC = gcc
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -g 
LDFLAGS += -lglut -lGLU -lG 

all: cube

cube: cube.c  
	$(CC) $(CFLAGS) $(LDFLAGS) -o cube cube.c

clean:
	rm -f *.o cube 

