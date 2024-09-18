CC=gcc
CFLAGS=-Wall

all: rush

rush: main.c builtin.o error.o
	$(CC) $(CFLAGS) -o rush main.c builtin.o error.o

builtin.o: builtin.c builtin.h
	$(CC) $(CFLAGS) -c builtin.c

error.o: error.c error.h
	$(CC) $(CFLAGS) -c error.c

clean:
	rm -f *.o rush
