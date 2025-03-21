# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Target executable
TARGET = shell

# Source files
SRCS = shell.c
OBJS = $(SRCS:.c=.o)

# Default target: Build the program
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean: rm -f $(OBJS) $(TARGET)

# Run the program with arguments
run: $(TARGET)
	./$(TARGET) $(ARGS)

# Phony targets (not actual files)
.PHONY: all clean run