CC = gcc
CFLAGS_COMMON = -Wpedantic -std=c11 -Wall -Wextra `sdl2-config --cflags`
LDFLAGS_COMMON = `sdl2-config --libs` -lSDL2_ttf -lSDL2_mixer -lm

# Обычная сборка (с оптимизацией)
CFLAGS_RELEASE = -O2 $(CFLAGS_COMMON)
LDFLAGS_RELEASE = $(LDFLAGS_COMMON)

# Сборка с санитайзерами (для отладки)
CFLAGS_SANITIZE = -O0 -g -fsanitize=address -fsanitize=undefined $(CFLAGS_COMMON)
LDFLAGS_SANITIZE = -fsanitize=address -fsanitize=undefined $(LDFLAGS_COMMON)

SRCDIR = src
SOURCES = $(SRCDIR)/2048.c $(SRCDIR)/game.c $(SRCDIR)/render.c $(SRCDIR)/audio.c $(SRCDIR)/leaderboard.c
TARGET = 2048
TARGET_SANITIZE = 2048-sanitize

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS_RELEASE) -o $@ $^ $(LDFLAGS_RELEASE)

sanitize: $(TARGET_SANITIZE)

$(TARGET_SANITIZE): $(SOURCES)
	$(CC) $(CFLAGS_SANITIZE) -o $@ $^ $(LDFLAGS_SANITIZE)

clean:
	rm -f $(TARGET) $(TARGET_SANITIZE)

.PHONY: all sanitize clean
