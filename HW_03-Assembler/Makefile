CFLAGS = -Wall -Wextra -Wpedantic -std=c11
LDFLAGS = 

all: solution
asm: asm-prog

asm-prog: main.o
	$(CC) -g -no-pie $^ -o $@
main.o: main.asm
	$(AS) -g -felf64 $^

debug: CFLAGS += --debug
debug: solution

solution: main.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	
clean:
	rm -f solution* asm-prog* core

.PHONY: all clean
