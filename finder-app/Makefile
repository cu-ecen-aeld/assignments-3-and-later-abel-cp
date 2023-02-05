CROSS_COMPILE := /opt/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
OBJ_FILES := writer.o
writer: writer.o
	$(CC) writer.o -o writer

writer.o: writer.c
	$(CC) -c writer.c -o writer.o

clean:
	rm -rf $(OBJ_FILES) writer

