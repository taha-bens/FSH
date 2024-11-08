.PHONY: clean all

# Compiler and flags
CC ?= gcc
CCFLAGS ?= -Wall -g3 -fsanitize=address

# Headers
HEADERS = pwd.h

# Shared libraries
SHLIBS ?= -lreadline 

# Target program
all: fsh

# Rule to build the executable
fsh: fsh.c pwd.o
	$(CC) $(CCFLAGS) -o $@ $^ $(SHLIBS)

# Rule to compile the source file into an object file
pwd.o: pwd.c
	$(CC) $(CCFLAGS) -c $< 

#pwd: pwd.o
#	$(CC) $(CCFLAGS) -o $@ $^

# Clean target
clean:
	rm -f fsh *.o
