.PHONY: clean all

# Compiler and flags
CC ?= gcc
CCFLAGS ?= -Wall -g3 -fsanitize=address

# Shared libraries
SHLIBS ?= -lreadline

# Target program
all: fsh

# Rule to build the executable
fsh: fsh.o
	$(CC) $(CCFLAGS) -o $@ $^ $(SHLIBS)

# Rule to compile the source file into an object file
fsh.o: fsh.c
	$(CC) $(CCFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f fsh *.o
