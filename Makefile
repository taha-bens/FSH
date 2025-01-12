.PHONY: clean all

# Compiler and flags
CC ?= gcc
CCFLAGS ?= -Wall  -Os  #-fsanitize=address #A désactiver pour les tests valgrind mais utile pour debug

# Headers
HEADERS = pwd.h ftype.h cd.h node.h stack_dir.h string_util.h ast.h execution.h 

# Shared libraries
SHLIBS ?= -lreadline 

# Target program
all: fsh

# Rule to build the executable
fsh: fsh.o node.o pwd.o ftype.o cd.o stack_dir.o string_util.o ast.o execution.o 
	$(CC) $(CCFLAGS) -o $@ $^ $(SHLIBS)

# Rule to compile the source file into an object file
fsh.o: fsh.c
	$(CC) $(CCFLAGS) -c $<

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

ast.o: ast.c
	$(CC) $(CCFLAGS) -c $<

execution.o: execution.c
	$(CC) $(CCFLAGS) -c $<

# Clean target
clean:
	rm -f fsh *.o
