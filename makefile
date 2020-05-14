CC = gcc 

INCLUDES = -I./include/libfdr
CFLAGS = -g $(INCLUDES) 
LIBS = $(INCLUDES)/libfdr.a 

EXECUTABLES: gsh

all: $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $*.c

gsh: gsh.o
	$(CC) $(CFLAGS) -g -o gsh gsh.o $(LIBS)

#make clean will rid your directory of the executable,
#object files, and any core dumps you've caused
clean:
	rm core $(EXECUTABLES) *.o


