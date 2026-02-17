CC = gcc
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -g
LDFLAGS += -lsqlite3 -lm

all: sqlite_job


sqlite_test: sqlite_job.c  
	$(CC) $(CFLAGS) $(LDFLAGS) -o sqlite_job sqlite_job.c 

clean:
	rm -f *.o sqlite_job 

