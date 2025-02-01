CC = gcc
CFLAGS = -Wall -g

all: client server

client: client.o network.o
	$(CC) $(CFLAGS) -o client client.o network.o

server: server.o
	$(CC) $(CFLAGS) -o server server.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

network.o: network.c
	$(CC) $(CFLAGS) -c network.c

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f *.o client server
