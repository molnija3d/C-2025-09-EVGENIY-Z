CC = gcc
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -g 
LDFLAGS += -lglut -lGLU -lGL -lm

all: cube

moon: cube_moon.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o cube_moon cube_moon.c

cube: cube.c  
	$(CC) $(CFLAGS) $(LDFLAGS) -o cube cube.c

clean:
	rm -f *.o cube cube_moon

