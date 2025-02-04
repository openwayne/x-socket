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

void* init_socks5_thread(void *arg);
void* init_http_thread(void *arg);
void* handle_socks5_proxy(void *arg);
void* handle_http_proxy(void *arg);
int forward_data(int from_sock, int to_sock);
int init_forward_socket(const char *host, int port);


#endif // __PROXY_H__
