CFLAGS = -Wall -Wextra -Wpedantic -std=c11
LDFLAGS = 

all: solution

debug: CFLAGS += --debug
debug: solution

solution: main.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	
clean:
	rm -f solution* core

.PHONY: all clean
