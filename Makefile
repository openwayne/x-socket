.PHONY: clean

CC = gcc
CFLAGS = -Wall -g

all: client server

client: client.o network.o
	$(CC) $(CFLAGS) -o client client.o network.o

server: server.o
	$(CC) $(CFLAGS) -o server server.o

client.o: client.c network.h
	$(CC) $(CFLAGS) -c client.c -o client.o

network.o: network.c network.h
	$(CC) $(CFLAGS) -c network.c -o network.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c	-o server.o


clean:
	rm -f *.o client server
