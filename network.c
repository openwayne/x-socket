#include "network.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void *recv_thread_func(void *arg)
{
    socket_t *s = (socket_t *)arg;
    while (1)
    {
        int len = recv(s->sockfd, s->recv_buffer, BUFFER_SIZE, 0);
        if (len == -1)
        {
            perror("recv");
            break;
        }
        pthread_mutex_lock(&s->recv_mutex);
        s->recv_len = len;
        pthread_mutex_unlock(&s->recv_mutex);
    }
    return NULL;
}

void *send_thread_func(void *arg)
{
    socket_t *s = (socket_t *)arg;
    while (1)
    {
        pthread_mutex_lock(&s->send_mutex);
        if (s->send_len > 0)
        {
            int len = send(s->sockfd, s->send_buffer, s->send_len, 0);
            if (len == -1)
            {
                perror("send");
                break;
            }
            s->send_len = 0;
        }
        pthread_mutex_unlock(&s->send_mutex);
    }
    return NULL;
}

int socket_init(socket_t *s, const char *proxy_ip, int proxy_port, int proxy_type)
{
    s->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    strcpy(s->proxy_ip, proxy_ip);
    s->proxy_port = proxy_port;
    s->proxy_type = proxy_type;

    struct sockaddr_in proxy_addr;
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(proxy_port);
    proxy_addr.sin_addr.s_addr = inet_addr(proxy_ip);

    if (connect(s->sockfd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) == -1)
    {
        perror("connect to proxy");
        close(s->sockfd);
        return -1;
    }

    pthread_mutex_init(&s->recv_mutex, NULL);
    pthread_mutex_init(&s->send_mutex, NULL);

    return 0;
}

int socket_start_recv(socket_t *s)
{
    return pthread_create(&s->recv_thread, NULL, recv_thread_func, s);
}

int socket_start_send(socket_t *s)
{
    return pthread_create(&s->send_thread, NULL, send_thread_func, s);
}

int socket_send(socket_t *s, const char *data, int len, const char *target_host, int target_port)
{
    pthread_mutex_lock(&s->send_mutex);
    if (len > BUFFER_SIZE)
    {
        len = BUFFER_SIZE;
    }

    if (s->proxy_type == 0)
    {
        // HTTP
        len = http_proxy_pack(data, len, s->send_buffer, BUFFER_SIZE, target_host, target_port);
    }
    else
    {
        // SOCKS
        len = socks5_proxy_pack(data, len, s->send_buffer, BUFFER_SIZE, target_host, target_port);
    }

    memcpy(s->send_buffer, data, len);
    s->send_len = len;

    send(s->sockfd, s->send_buffer, len, 0);

    pthread_mutex_unlock(&s->send_mutex);
    return 0;
}

int socket_recv(socket_t *s, char *data, int len)
{
    pthread_mutex_lock(&s->recv_mutex);
    int origin_data_len = recv(s->sockfd, s->recv_buffer, BUFFER_SIZE, 0);
    int msg_len = 0;

    if (origin_data_len == 0)
    {
        pthread_mutex_unlock(&s->recv_mutex);
        perror("recv_len == 0\n");
        return 0;
    }

    if (s->proxy_type == 0)
    {
        // HTTP
        msg_len = http_proxy_unpack(s->recv_buffer, origin_data_len, data, len);
    }
    else
    {
        // SOCKS
        msg_len = socks5_proxy_unpack(s->recv_buffer, origin_data_len, data, len);
    }

    memcpy(data, s->recv_buffer, msg_len);
    s->recv_len = msg_len;

    pthread_mutex_unlock(&s->recv_mutex);
    return len;
}

int socket_close(socket_t *s)
{
    pthread_cancel(s->recv_thread);
    pthread_cancel(s->send_thread);
    pthread_join(s->recv_thread, NULL);
    pthread_join(s->send_thread, NULL);
    close(s->sockfd);
    pthread_mutex_destroy(&s->recv_mutex);
    pthread_mutex_destroy(&s->send_mutex);
    return 0;
}

int http_proxy_pack(const char *data, int len, char *buffer, int buffer_size, const char *target_host, int target_port)
{
    char request_line[256];
    sprintf(request_line, "CONNECT %s:%d HTTP/1.1\r\n", target_host, target_port);

    printf("request_line: %s\n", request_line);

    char headers[256];
    sprintf(headers, "Host: %s\r\n\r\n", target_host);

    printf("headers: %s\n", headers);

    int request_len = strlen(request_line);
    int headers_len = strlen(headers);

    if (request_len + headers_len + len > buffer_size)
    {
        return -1;
    }

    memcpy(buffer, request_line, request_len);
    memcpy(buffer + request_len, headers, headers_len);
    memcpy(buffer + request_len + headers_len, data, len);

    return request_len + headers_len + len;
}

int http_proxy_unpack(const char *buffer, int len, char *data, int data_size)
{
    char *status_line = strstr(buffer, "\r\n");
    if (status_line == NULL)
    {
        return -1;
    }

    int header_len = status_line - buffer + 2;
    int data_len = len - header_len;

    if (data_len > data_size)
    {
        data_len = data_size;
    }

    memcpy(data, buffer + header_len, data_len);

    return data_len;
}

int socks5_proxy_pack(const char *data, int len, char *buffer, int buffer_size, const char *target_host, int target_port)
{
    // Construct SOCKS5 request
    unsigned char request[BUFFER_SIZE];
    int request_len = 0;

    // Version number
    request[request_len++] = 0x05;

    // Request command: CONNECT
    request[request_len++] = 0x01;

    // Reserved field
    request[request_len++] = 0x00;

    // Target address type: domain name
    request[request_len++] = 0x03;

    // Target address
    request[request_len++] = strlen(target_host);
    memcpy(request + request_len, target_host, strlen(target_host));
    request_len += strlen(target_host);

    // Target port
    memcpy(request + request_len, &target_port, 2);
    request_len += 2;

    // Data
    memcpy(request + request_len, data, len);
    request_len += len;

    // Copy to buffer
    if (request_len > buffer_size)
    {
        return -1; // Buffer insufficient
    }
    memcpy(buffer, request, request_len);

    return request_len;
}

int socks5_proxy_unpack(const char *buffer, int len, char *data, int data_size)
{
    // Parse SOCKS5 response
    if (len < 4)
    {
        return -1; // Response format error
    }

    // Check version number
    if (buffer[0] != 0x05)
    {
        return -1; // Version number error
    }

    // Check response status
    if (buffer[1] != 0x00)
    {
        return -1; // Response status error
    }

    // Get data length
    int data_len = len - 4;

    // Check data length
    if (data_len > data_size)
    {
        data_len = data_size;
    }

    // Copy data to output buffer
    memcpy(data, buffer + 4, data_len);

    return data_len;
}
