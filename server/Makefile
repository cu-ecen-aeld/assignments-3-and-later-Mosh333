
# Makefile for /server dir
# Keeping it simple for this time around

CC:=${CROSS_COMPILE}gcc
CFLAGS = -Wall -Wextra -Werror -g
LDFLAGS = -lpthread -lrt

default: all

all:
	${CC} -o aesdsocket *.c ${CFLAGS} ${LDFLAGS}

clean:
	rm -rf aesdsocket *.o *.elf *.map

.PHONY: all default clean