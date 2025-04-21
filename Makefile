# https://github.com/lfe/lfe/blob/2fb4de84f81ac822923ddc60eaccc4a938c9dfdc/Makefile#L68
# https://github.com/heiher/hev-socks5-server/blob/778997fca845707dbc13bf052030fe679730de01/Makefile#L37
CC = gcc # Compiler
CFLAGS = -Wall -Wextra -g -Iinclude  # Compiler flags

# Target file
TARGET = shell

# Source code folder and build folder
SRCDIR = src
INCDIR = include
BUILDDIR = build
SRCS = $(wildcard $(SRCDIR)/*.c)  # Find all .c files in src/
OBJS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))  # Convert .c files to .o files

# Default target, set dependencies with all, clean, run
# If file does not exist or we need update him
# make will run the $(TARGET) rule for creation the target file
all: $(BUILDDIR) $(TARGET)

# Create build directory if it doesn't exist
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# make, compile the target file

# Rule for: $(TARGET)
# Build the target file (./shell) with the CFLAGS files and OBJS files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile the .c files to .o files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Exmple
# build/main.o: src/main.c
#     gcc -Wall -Wextra -g -Iinclude -c src/main.c -o build/main.o

# Clean up builded .o files
clean:
	rm -rf $(BUILDDIR) $(TARGET)

# make clean, remove all .o files and the target file

# Run the program with arguments, ARGS is the arguments provided by the user
run: $(TARGET)
	./$(TARGET) $(ARGS)

# make run ARGS="-c -p 8071"

# all, clean, run are not files. Prevent coflict if there are files with the same name
.PHONY: all clean run $(BUILDDIR)