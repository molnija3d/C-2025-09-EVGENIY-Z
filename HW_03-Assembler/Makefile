CFLAGS = -Wall -Wextra -Wpedantic -std=c11
LDFLAGS = 

all: solution
asm: asm-prog
optimised: main-optimised.o

asm-prog: main.o
	$(CC) -g -no-pie $^ -o $@
main.o: main.asm
	$(AS) -g -felf64 $^
	
optimised: main-optimised.o
	$(CC) -g -no-pie $^ -o $@
main-optimised.o: main-optimised.asm
	$(AS) -g -felf64 $^

debug: CFLAGS += --debug
debug: solution

solution: main.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	
clean:
	rm -f solution* asm-prog* optimised* core

.PHONY: all clean
