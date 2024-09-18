CC=gcc
CFLAGS=-Wall -std=c99
DOCKER_IMAGE=ubuntu-gcc-11
DOCKER_CONTAINER=ubuntu-gcc-11-container
all: rush

rush: main.c builtin.o error.o
	$(CC) $(CFLAGS) -o rush main.c builtin.o error.o

builtin.o: builtin.c builtin.h
	$(CC) $(CFLAGS) -c builtin.c

error.o: error.c error.h
	$(CC) $(CFLAGS) -c error.c

clean:
	rm -f *.o rush

build:
	docker build -t $(DOCKER_IMAGE)

run:
	docker run --name $(DOCKER_CONTAINER) -it --rm -v $(PWD):/workspace $(DOCKER_IMAGE)

remove:
	docker stop $(DOCKER_CONTAINER)
	docker rm $(DOCKER_CONTAINER)

