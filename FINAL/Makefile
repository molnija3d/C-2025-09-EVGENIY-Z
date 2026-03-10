CC = gcc
CFLAGS = -Wpedantic -std=c11 -Wall -Wextra -g -I. 
LDFLAGS = -lssl -lcrypto -lcurl 

SRCS = main.c utils.c bencode.c torrent.c tracker.c network.c peer.c storage.c tar.c config.c
OBJS = $(SRCS:.c=.o)
TARGET = torrent_client
TARGET_SANITIZE = torrent_client_sanitize

CFLAGS_SANITIZE = -O0 -g -fsanitize=address -fsanitize=undefined $(CFLAGS_COMMON)
LDFLAGS_SANITIZE = $(LDFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sanitize: $(TARGET_SANITIZE) 

$(TARGET_SANITIZE): $(OBJS)
	$(CC) $(CFLAGS_SANITIZE) -o $@ $^ $(LDFLAGS_SANITIZE)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET_SANITIZE) 

.PHONY: clean
