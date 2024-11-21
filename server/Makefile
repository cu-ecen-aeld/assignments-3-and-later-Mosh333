
# Makefile for /server dir
# Keeping it simple for this time around

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -Werror -g
LDFLAGS ?= -lpthread -lrt
INCLUDES ?=
TARGET ?= aesdsocket

# Default target
default: $(TARGET)

# Build target
$(TARGET): *.c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o

# Declare 'default' and 'clean' as phony targets
.PHONY: default clean
