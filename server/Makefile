CROSS_COMPILE := 
#CROSS_COMPILE := aarch64-none-linux-gnu-
#CROSS_COMPILE := /opt/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -g -Og -Wall -Werror
OBJ_FILES := aesdsocket.o
aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

.PHONY: all clean
all: aesdsocket

clean:
	rm -rf aesdsocket aesdsocket.o

