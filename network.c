#include "network.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "queue.h"

void *recv_thread_func(void *arg)
{
    socket_t *s = (socket_t *)arg;
    char recv_data[BUFFER_SIZE];
    while (1)
    {
        printf("recv_thread_func\n");
        int recv_len = recv(s->sockfd, recv_data, BUFFER_SIZE, 0);

        int error = socket_recv(s, recv_data, recv_len);
        if (error == -1)
        {
            perror("recv thread error!!");
            break;
        }
        memset(recv_data, 0, BUFFER_SIZE);
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

    if(s->proxy_type == SOCKS5_CONNECT) {
        if (send_socks5_greeting(s) == -1)
        {
            perror("send_socks5_greeting");
            return -1;
        }

        s->socks5_state = SOCKS5_GREETING;

        if (receive_socks5_greeting_response(s) == -1)
        {
            perror("receive_socks5_greeting_response");
            return -1;
        }

        s->socks5_state = SOCKS5_GREETING_RESPONSE;

        if (send_socks5_connection_request(s, "www.baidu.com", 80) == -1)
        {
            perror("send_socks5_connection_request");
            return -1;
        }

        s->socks5_state = SOCKS5_GREETING_RESPONSE;

        if (receive_socks5_connection_reply(s) == -1)
        {
            perror("receive_socks5_connection_reply");
            return -1;
        }
        s->socks5_state = SOCKS5_GREETING_RESPONSE;
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

    s->send_len = http_proxy_pack(data, len, s->send_buffer, BUFFER_SIZE, target_host, target_port);


    if (s->send_len == -1)
    {
        pthread_mutex_unlock(&s->send_mutex);
        perror("send buffer insufficient");
        return -1;
    }

    printf("Send origin data: %s\n", data);
    printf("Send data: %s\n", s->send_buffer);
    printf("Send data Hex: ");
    for (int i = 0; i < s->send_len; i++)
    {
        printf("%02X ", s->send_buffer[i]);
    }
    printf("\nSend data length: %d\n\n\n", s->send_len);


    send(s->sockfd, s->send_buffer, s->send_len, 0);

    pthread_mutex_unlock(&s->send_mutex);
    return 0;
}

int socket_recv(socket_t *s, const char *data, int len)
{
    pthread_mutex_lock(&s->recv_mutex);
    printf("Received origin data: %s\n", data);
    printf("Received data Hex: ");
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
    }
    printf("\nReceived data length: %d\n\n\n", len);

    http_message_t* msg = http_proxy_unpack(data, len);
    if (msg == NULL)
    {
        pthread_mutex_unlock(&s->recv_mutex);
        return RECV_FAILED;
    }

    enqueue(&s->recv_queue, msg, s->proxy_type);

    pthread_mutex_unlock(&s->recv_mutex);
    return RECV_SUCCESS;
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

http_message_t* http_proxy_unpack(const char *buffer, int len)
{
    printf("buffer: %s\n", buffer);
    http_message_t* message = malloc(sizeof(http_message_t));

    // Parse request line
    char *line_end = strstr(buffer, "\r\n");
    if (line_end == NULL)
    {
        perror("Request line not found");
        return NULL; // Request line not found
    }
    int line_len = line_end - buffer;
    if (line_len >= sizeof(message->method))
    {
        perror("Method too long");
        return NULL; // Method too long
    }
    memcpy(message->method, buffer, line_len);
    message->method[line_len] = '\0';

    // Parse headers
    char *headers_start = line_end + 2;
    char *headers_end = strstr(headers_start, "\r\n\r\n");
    if (headers_end == NULL)
    {
        perror("Headers not found");
        return NULL; // Headers not found
    }
    int headers_len = headers_end - headers_start;
    if (headers_len >= sizeof(message->headers))
    {
        perror("Headers too long");
        return NULL; // Headers too long
    }
    memcpy(message->headers, headers_start, headers_len);
    message->headers[headers_len] = '\0';

    // Parse body
    char *body_start = headers_end + 4;
    int body_len = len - (body_start - buffer);
    if (body_len >= sizeof(message->body))
    {
        perror("Body too long");
        return NULL; // Body too long
    }
    memcpy(message->body, body_start, body_len);
    message->body[body_len] = '\0';

    printf("http_message_t:\n");
    printf("method: %s\n", message->method);
    printf("uri: %s\n", message->uri);
    printf("version: %s\n", message->version);
    printf("headers: %s\n", message->headers);
    printf("body: %s\n\n\n", message->body);

    return message;
}

int send_socks5_greeting(socket_t *s)
{
    socks5_greeting_request_t greeting_req;
    greeting_req.version = SOCKS5_VERSION;
    greeting_req.nmethods = SOCKS5_NO_AUTH_METHOD;  // No authentication method
    greeting_req.methods[0] = SOCKS5_NO_AUTH;  // No authentication

    int len = send(s->sockfd, &greeting_req, sizeof(greeting_req), 0);
    if (len == -1)
    {
        perror("Failed to send SOCKS5 greeting request");
        return -1;
    }

    return 0;
}

int receive_socks5_greeting_response(socket_t *s)
{
    socks5_greeting_response_t response;
    int len = recv(s->sockfd, &response, sizeof(response), 0);
    if (len == -1)
    {
        perror("Failed to receive SOCKS5 greeting response");
        return -1;
    }

    if (response.version != SOCKS5_VERSION)
    {
        perror("Invalid SOCKS5 version");
        return -1;
    }

    if (response.method != 0x00)
    {
        perror("Invalid SOCKS5 method");
        return -1;
    }

    printf("receive_socks5_greeting_response\n");
    printf("response.version: %d\n", response.version);
    printf("response.method: %d\n\n\n", response.method);

    return 0;
}

int send_socks5_connection_request(socket_t *s, const char *target_host, int target_port)
{
    socks5_request_t request;
    request.version = SOCKS5_VERSION;
    request.command = 0x01;  // CONNECT command
    request.reserved = 0x00;
    request.address_type = 0x03;  // Domain name address type

    uint8_t domain_len = strlen(target_host);
    request.dst_addr.domain.length = domain_len;
    strncpy(request.dst_addr.domain.domain, target_host, domain_len);
    request.dst_port = htons(target_port);  // Network byte order

    ssize_t bytes_sent = send(s->sockfd, &request, sizeof(request) - sizeof(request.dst_addr) + domain_len + 1, 0);
    if (bytes_sent < 0) {
        perror("Failed to send SOCKS5 connection request");
        return -1;
    }
    return 0;
}

int receive_socks5_connection_reply(socket_t *s)
{
    socks5_response_t response;
    int len = recv(s->sockfd, &response, sizeof(response), 0);
    if (len == -1)
    {
        perror("Failed to receive SOCKS5 connection reply");
        return -1;
    }

    if (response.version != SOCKS5_VERSION)
    {
        perror("Invalid SOCKS5 version");
        return -1;
    }

    if (response.reply != 0x00)
    {
        perror("SOCKS5 connection reply error");
        return -1;
    }

    return 0;
}
