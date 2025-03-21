# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude  # Добавляем include для заголовочных файлов

# Target executable
TARGET = shell

# Source and object files
SRCDIR = src
INCDIR = include
BUILDDIR = build
SRCS = $(wildcard $(SRCDIR)/*.c)  # Автоматически находим все .c файлы в src/
OBJS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))  # Преобразуем .c файлы в .o и сохраняем в build/

# Default target: Build the program
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile source files into object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(BUILDDIR)/*.o $(TARGET)

# Run the program with arguments
run: $(TARGET)
	./$(TARGET) $(ARGS)

# Phony targets (not actual files)
.PHONY: all clean run