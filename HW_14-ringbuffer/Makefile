obj-m += ring_buffer_char.o

EXTRA_CFLAGS += -Wall
EXTRA_CFLAGS += -Wextra
EXTRA_CFLAGS += -Wno-unused-parameter      
EXTRA_CFLAGS += -Wno-missing-field-initializers
EXTRA_CFLAGS += -Wframe-larger-than=1024   
EXTRA_CFLAGS += -Wno-maybe-uninitialized   

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod ring_buffer_char.ko

uninstall:
	sudo rmmod ring_buffer_char

load: all install
	sudo dmesg -C
	sudo insmod ring_buffer_char.ko
	sudo dmesg | tail -10

test:
	# Создаем тестовые программы
	gcc -o test_write test_write.c
	gcc -o test_read test_read.c
	gcc -o test_ioctl test_ioctl.c
