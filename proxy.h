#ifndef __PROXY_H__
#define __PROXY_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/select.h>

#define BUFFER_SIZE 4096
#define SOCKS5_PORT 8777
#define HTTP_PORT 8778

void *initSocks5Thread(void *arg);
void *initHttpThread(void *arg);
void *handleSocks5Proxy(void *arg);
void *handleHttpProxy(void *arg);
int forwardData(int fromSock, int toSock);
int initForwardSocket(const char *host, int port);

int socks5_connect(int sock, const char *host, int port);

#endif // __PROXY_H__
