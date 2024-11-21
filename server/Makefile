
# Makefile for /server dir
# Flexible for native and cross-compilation

# Compiler and flags
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -Werror -g
LIB := -lpthread -lrt
LDFLAGS ?=
INCLUDES :=

# Target and source definitions
TARGET := aesdsocket
SRCS := aesdsocket.c
OBJS := $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Build the target application
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIB) $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) $(OBJS)

# Declare 'all' and 'clean' as phony targets
.PHONY: all clean

