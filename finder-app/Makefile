CROSS_COMPILE := aarch64-none-linux-gnu-
#CROSS_COMPILE := /opt/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
OBJ_FILES := writer.o
writer: writer.c
	$(CC) writer.c -o writer
.PHONY: all clean
all: writer

clean:
	rm -rf writer

