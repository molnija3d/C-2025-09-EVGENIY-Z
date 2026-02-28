CC = gcc
CFLAGS += -Wall -Wextra -O0 -Wall -Wextra -Wpedantic -std=c11 -g `sdl2-config --cflags` -fsanitize=address -fsanitize=undefined
LDFLAGS += `sdl2-config --libs` -lSDL2_ttf -lm -lSDL2_mixer -fsanitize=address -fsanitize=undefined
SRCDIR = src
SOURCES = $(SRCDIR)/2048.c  $(SRCDIR)/game.c $(SRCDIR)/render.c $(SRCDIR)/audio.c $(SRCDIR)/leaderboard.c
TARGET = 2048

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
