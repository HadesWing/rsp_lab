# 板上原生编译（用户态程序，不需要内核源码树）
CC      ?= gcc
CFLAGS  ?= -O2 -Wall

all: spi_loop

spi_loop: spi_loop.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f spi_loop

.PHONY: all clean
