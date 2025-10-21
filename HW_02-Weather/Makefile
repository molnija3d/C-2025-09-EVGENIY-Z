
all: solution

solution: main.c
	$(CC) $^ -o $@ -L ./LIBS -lcjson -lcurl -Wall -Wextra -Wpedantic -std=c11

clean:
	rm -f solution core

.PHONY: all clean
