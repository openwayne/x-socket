.PHONY: clean

CC = gcc
CFLAGS = -Wall -g -lpthread -fPIC

SRCS = proxy.c proxy_server.c
OBJS = $(SRCS:.c=.o)
HDRS = proxy.h

LDFLAGS = -shared

all: $(OBJS) proxy_server libproxy.so process_main

libproxy.so: proxy.o
	$(CC) $(LDFLAGS) -o libproxy.so proxy.o

proxy_server: proxy.o proxy_server.o
	gcc proxy.o proxy_server.o -o proxy_server -lpthread

process_main: process_main.o linked_list.o process.o
	gcc process_main.o linked_list.o process.o -o process_main

$(OBJS): $(SRCS) $(HDRS)
	gcc $(CFLAGS) -c $(SRCS)

clean:
	rm -f $(TARGET) $(OBJS) proxy_server libproxy.so
