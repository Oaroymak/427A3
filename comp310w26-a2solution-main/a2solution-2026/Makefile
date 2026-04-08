CC=gcc
#CFLAGS=-g -O0 -Wall #-DNDEBUG
CFLAGS=-DNDEBUG -Wall

# A3: Accept framesize and varmemsize as make arguments
# Usage: make mysh framesize=18 varmemsize=10
framesize ?= 18
varmemsize ?= 10

DEFINES = -DFRAME_STORE_SIZE=$(framesize) -DVAR_MEM_SIZE=$(varmemsize)

mysh: shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) $(DEFINES) -c shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) $(DEFINES) -o mysh shell.o interpreter.o shellmemory.o pcb.o queue.o schedule_policy.o -lpthread

clean:
	rm -f mysh *.o
