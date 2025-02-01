#ifndef __NETWORK_H__
#define __NETWORK_H__


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// buffer size
#define BUFFER_SIZE 1024

typedef struct {
    int sockfd;
    pthread_t recv_thread;
    pthread_t send_thread;
    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    int recv_len;
    int send_len;
    pthread_mutex_t recv_mutex;
    pthread_mutex_t send_mutex;
    char proxy_ip[16];
    int proxy_port;
    int proxy_type; // 0: HTTP, 1: SOCKS
} socket_t;

int socket_init(socket_t *s, const char *proxy_ip, int proxy_port, int proxy_type);

int socket_start_recv(socket_t *s);

int socket_start_send(socket_t *s);

int socket_send(socket_t *s, const char *data, int len, const char *target_host, int target_port);

int socket_recv(socket_t *s, char *data, int len);

int socket_close(socket_t *s);

int http_proxy_send(socket_t *s, const char *data, int len);

int http_proxy_unpack(const char *buffer, int len, char *data, int data_size);

int socks5_proxy_pack(const char *data, int len, char *buffer, int buffer_size, const char *target_host, int target_port);

int socks5_proxy_unpack(const char *buffer, int len, char *data, int data_size);

#endif // __NETWORK_H__
