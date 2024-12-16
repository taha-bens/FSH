.PHONY: clean all

# Compiler and flags
CC ?= gcc
CCFLAGS ?= -Wall #-fsanitize=address #A désactiver pour les tests valgrind mais utile pour debug

# Headers
HEADERS = pwd.h ftype.h cd.h for.h node.h stack_dir.h string_util.h

# Shared libraries
SHLIBS ?= -lreadline 

# Target program
all: fsh

# Rule to build the executable
fsh: fsh.c node.o pwd.o ftype.o cd.o for.o stack_dir.o string_util.o
	$(CC) $(CCFLAGS) -o $@ $^ $(SHLIBS)

# Rule to compile the source file into an object file
node.o: node.c
	$(CC) $(CCFLAGS) -c $<

stack_dir.o: stack_dir.c
	$(CC) $(CCFLAGS) -c $<

string_util.o: string_util.c
	$(CC) $(CCFLAGS) -c $<

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
