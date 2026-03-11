# Компилятор и флаги
CC = gcc
CFLAGS = -Wpedantic -std=c11 -Wall -Wextra -g -Iheaders
LDFLAGS = -lssl -lcrypto -lcurl

# Флаги для сборки с санитайзерами
CFLAGS_SANITIZE = -O0 -g -fsanitize=address -fsanitize=undefined -Iheaders
LDFLAGS_SANITIZE = $(LDFLAGS) -fsanitize=address -fsanitize=undefined

# Директории
SRC_DIR = src
HEADER_DIR = headers
BUILD_DIR = builds
BUILD_SANITIZE_DIR = $(BUILD_DIR)/sanitize

# Исходные файлы (лежат в src/)
SRCS = main.c utils.c bencode.c torrent.c tracker.c network.c peer.c storage.c tar.c config.c
# Полные пути к исходникам
SRCS := $(addprefix $(SRC_DIR)/, $(SRCS))

# Объектные файлы для обычной сборки (в builds/)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
# Объектные файлы для сборки с санитайзерами (в builds/sanitize/)
OBJS_SANITIZE = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_SANITIZE_DIR)/%.o)

# Исполняемые файлы
TARGET = torrent_client
TARGET_SANITIZE = torrent_client_sanitize
TARGET := $(addprefix $(BUILD_DIR)/, $(TARGET))
TARGET_SANITIZE := $(addprefix $(BUILD_DIR)/, $(TARGET_SANITIZE))

# Цели по умолчанию
.PHONY: all clean sanitize

all: $(TARGET)

# Сборка обычной версии
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Сборка версии с санитайзерами
sanitize: $(TARGET_SANITIZE)

$(TARGET_SANITIZE): $(OBJS_SANITIZE) | $(BUILD_DIR)
	$(CC) $(CFLAGS_SANITIZE) -o $@ $^ $(LDFLAGS_SANITIZE)

# Правила компиляции объектных файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_SANITIZE_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_SANITIZE_DIR)
	$(CC) $(CFLAGS_SANITIZE) -c $< -o $@

# Создание необходимых директорий
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_SANITIZE_DIR): | $(BUILD_DIR)
	mkdir -p $(BUILD_SANITIZE_DIR)

# Очистка
clean:
	rm -rf $(BUILD_DIR)
