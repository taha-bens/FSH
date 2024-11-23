.PHONY: clean all

# Compiler and flags
CC ?= gcc
CCFLAGS ?= -Wall -g3 -fsanitize=address -fsanitize=undefined -fsanitize=leak

# Headers
HEADERS = pwd.h ftype.h cd.h for.h

# Shared libraries
SHLIBS ?= -lreadline 

# Target program
all: fsh

# Rule to build the executable
fsh: fsh.c pwd.o ftype.o cd.o for.o
	$(CC) $(CCFLAGS) -o $@ $^ $(SHLIBS)

# Rule to compile the source file into an object file
pwd.o: pwd.c
	$(CC) $(CCFLAGS) -c $< 

ftype.o: ftype.c
	$(CC) $(CCFLAGS) -c $<

cd.o: cd.c
	$(CC) $(CCFLAGS) -c $<

for.o: for.c
	$(CC) $(CCFLAGS) -c $<

# Clean target
clean:
	rm -f fsh *.o
