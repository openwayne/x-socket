#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "queue.h"

#define MAX_HEADER_SIZE 1024
#define MAX_BODY_SIZE 1024

typedef struct
{
    char method[16];               // request method, such as GET, POST
    char uri[1024];                // request uri, such as /index.html
    char version[16];              // http version, such as HTTP/1.1
    char headers[MAX_HEADER_SIZE]; // request headers
    char body[MAX_BODY_SIZE];      // request body
} http_message_t;

// buffer size
#define BUFFER_SIZE 1024

typedef struct
{
    int sockfd;
    pthread_t recv_thread;
    pthread_t send_thread;
    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    Queue send_queue;
    Queue recv_queue;
    int recv_len;
    int send_len;
    pthread_mutex_t recv_mutex;
    pthread_mutex_t send_mutex;
    char proxy_ip[16];
    int proxy_port;
    int proxy_type; // 0: HTTP, 1: SOCKS
    int socks5_state;
} socket_t;

enum CONNECT_TYPE
{
    HTTP_CONNECT = 0,
    SOCKS5_CONNECT = 1,
};

enum DATA_RECV_STATE
{
    RECV_SUCCESS = 0,
    RECV_FAILED = -1,
};

int socket_init(socket_t *s, const char *proxy_ip, int proxy_port, int proxy_type);

int socket_start_recv(socket_t *s);

int socket_start_send(socket_t *s);

int socket_send(socket_t *s, const char *data, int len, const char *target_host, int target_port);

int socket_recv(socket_t *s, const char *data, int len);

int socket_close(socket_t *s);

int http_proxy_pack(const char *data, int len, char *buffer, int buffer_size, const char *target_host, int target_port);

int http_proxy_send(socket_t *s, const char *data, int len);

http_message_t *http_proxy_unpack(const char *buffer, int len);

// Define SOCKS5 structures
#define SOCKS5_VERSION 0x05
#define SOCKS5_NO_AUTH_METHOD 1
#define SOCKS5_NO_AUTH 0x00
#define SOCKS5_SUCCESS 0x00

typedef struct __attribute__((packed))
{
    uint8_t version;   // SOCKS version (0x05)
    uint8_t nmethods;  // Number of supported authentication methods
    uint8_t methods[]; // Supported authentication methods (list)
} socks5_greeting_request_t;

typedef struct __attribute__((packed))
{
    uint8_t version; // SOCKS version (0x05)
    uint8_t method;  // Selected authentication method
} socks5_greeting_response_t;

typedef struct __attribute__((packed))
{
    uint8_t version;      // SOCKS version (0x05)
    uint8_t command;      // Command code (e.g., 0x01 for CONNECT)
    uint8_t reserved;     // Reserved (must be 0x00)
    uint8_t address_type; // Address type (0x01, 0x03, or 0x04)
    union
    {
        struct
        {
            uint8_t ip[4];
        } ipv4; // IPv4 address (4 bytes)
        struct
        {
            uint8_t ip[16];
        } ipv6; // IPv6 address (16 bytes)
        struct
        {
            uint8_t length;
            char domain[255];
        } domain; // Domain name
    } dst_addr;
    uint16_t dst_port; // Destination port (network byte order)
} socks5_request_t;

typedef struct __attribute__((packed))
{
    uint8_t version;      // SOCKS version (0x05)
    uint8_t reply;        // Reply code (0x00 for success)
    uint8_t reserved;     // Reserved (must be 0x00)
    uint8_t address_type; // Address type (IPv4: 0x01, IPv6: 0x04, Domain: 0x03)
    union
    {
        struct
        {
            uint8_t ip[4];
        } ipv4; // IPv4 bound address
        struct
        {
            uint8_t ip[16];
        } ipv6; // IPv6 bound address
        struct
        {
            uint8_t length;
            char domain[255];
        } domain; // Domain name
    } bnd_addr;
    uint16_t bnd_port; // Bound port (network byte order)
} socks5_response_t;

// for socks5
int send_socks5_greeting(socket_t *s);
int receive_socks5_greeting_response(socket_t *s);
int send_socks5_connection_request(socket_t *s, const char *target_host, int target_port);
int receive_socks5_connection_reply(socket_t *s);

enum SOCKS5_STATE
{
    SOCKS5_GREETING = 0,
    SOCKS5_GREETING_RESPONSE = 1,
    SOCKS5_CONNECTION = 2,
    SOCKS5_CONNECTION_REPLY = 3,
};

#endif // __NETWORK_H__
