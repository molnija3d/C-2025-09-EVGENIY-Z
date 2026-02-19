CC = gcc
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -g -I/usr/include/libmongoc-1.0/ -I/usr/include/libbson-1.0/
LDFLAGS += -lsqlite3 -lm -lmongoc-1.0 -lbson-1.0 -lrt

all: test_db_job

sqlite_test: test_db_job.c  
	$(CC) $(CFLAGS) $(LDFLAGS) -o test_db_job test_db_job.c 

clean:
	rm -f *.o test_db_job 

