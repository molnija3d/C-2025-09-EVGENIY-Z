CC = gcc
CFLAGS += -Wall -Wextra -O2 -Wall -Wextra -Wpedantic -std=c11 -g `sdl2-config --cflags`  
LDFLAGS += `sdl2-config --libs` -lSDL2_ttf -lm -lSDL2_mixer
SRCDIR = src
SOURCES = $(SRCDIR)/2048.c  $(SRCDIR)/game.c $(SRCDIR)/render.c $(SRCDIR)/audio.c 
TARGET = 2048

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
