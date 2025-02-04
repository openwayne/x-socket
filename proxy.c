#include "proxy.h"

void* init_http_thread(void *arg) {
    int *return_value = malloc(sizeof(int));
    *return_value = 0;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(HTTP_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        *return_value = -1;
        return (void *)return_value;
    }

    listen(server_fd, 10);
    printf("HTTP Proxy listening on port %d\n", HTTP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        void* handle_status;
        socklen_t addr_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_http_proxy, client_sock);

        pthread_join(thread, &handle_status);
        *return_value = *(int *)handle_status;
        free(handle_status);
    }

    return (void *)return_value;
}

void* init_socks5_thread(void *arg) {
    int *return_value = malloc(sizeof(int));
    *return_value = 0;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SOCKS5_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return NULL;
    }

    listen(server_fd, 10);
    printf("SOCKS5 Proxy listening on port %d\n", SOCKS5_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        void* handle_status;
        socklen_t addr_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_socks5_proxy, client_sock);
        
        pthread_join(thread, &handle_status);
        *return_value = *(int *)handle_status;

        free(client_sock);
        free(handle_status);
    }

    return (void*)return_value;
}

void* handle_socks5_proxy(void *arg) {
    int client_sock = *(int *)arg;
    printf("client_sock: %d\n", client_sock);
    unsigned char buffer[BUFFER_SIZE];

    int *return_value = malloc(sizeof(int));
    *return_value = 0;
    
    // read greeting
    ssize_t len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    printf("Received HEX: ");
    for (int i = 0; i < len; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\nReceived data length: %ld\n\n\n", len);
    if (len < 3 || buffer[0] != 0x05) {
        close(client_sock);
        *return_value = -1;
        return (void *)return_value;
    }

    // return no authentication required
    unsigned char auth_response[] = {0x05, 0x00};
    send(client_sock, auth_response, 2, 0);
    // read request
    len = recv(client_sock, buffer, BUFFER_SIZE, 0);

    printf("Received HEX: ");
    for (int i = 0; i < len; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\nReceived data length: %ld\n\n\n", len);


    if (len < 7 || buffer[1] != 0x01) { // 只处理 CONNECT
        close(client_sock);
        *return_value = -1;
        return (void *)return_value;
    }

    // parse target address
    char host[256];
    int port;
    switch (buffer[3]) {
        case 0x01: // IPv4
            inet_ntop(AF_INET, &buffer[4], host, INET_ADDRSTRLEN);
            port = ntohs(*(unsigned short*)(buffer + 8));
            break;
        case 0x03: // Domain
            memcpy(host, buffer + 5, buffer[4]);
            host[buffer[4]] = '\0';
            port = ntohs(*(unsigned short*)(buffer + 5 + buffer[4]));
            break;
        default:
            close(client_sock);
            *return_value = -1;
            return (void *)return_value;
    }

    // connec to target
    int target_sock = init_forward_socket(host, port);

    if (target_sock < 0) {
        close(client_sock);
        close(target_sock);
        *return_value = -1;
        return (void *)return_value;
    }

    // response success
    unsigned char response[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    send(client_sock, response, 10, 0);

    // bidirectional data forwarding
    forward_data(target_sock, client_sock);

    close(target_sock);
    close(client_sock);
    return (void *)return_value;
}

void* handle_http_proxy(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    int *return_value = malloc(sizeof(int));
    *return_value = 0;

    // 读取请求头
    bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        *return_value = -1;
        return (void *)return_value;
    }
    buffer[bytes_read] = '\0';

    printf("Received data: \n%s\n\n\n", buffer);

    // 解析 Host
    char *host_start = strstr(buffer, "Host: ");
    if (!host_start) {
        close(client_socket);
        *return_value = -1;
        return (void *)return_value;
    }
    host_start += 6;
    char *host_end = strstr(host_start, "\r\n");
    if (!host_end) {
        close(client_socket);
        *return_value = -1;
        return (void *)return_value;
    }
    *host_end = '\0';

    // 提取端口
    char *port_str = strchr(host_start, ':');
    int port = port_str ? atoi(port_str + 1) : 80;
    char host[256];
    strncpy(host, host_start, port_str ? port_str - host_start : host_end - host_start);
    host[port_str ? port_str - host_start : host_end - host_start] = '\0';

    printf("Host: %s, Port: %d\n", host, port);

    // create target socket
    int target_sock = init_forward_socket(host, port);

    if (target_sock < 0) {
        close(client_socket);
        close(target_sock);
        *return_value = -1;
        return (void *)return_value;
    }

    // resolve CONNECT request
    if (strncmp(buffer, "CONNECT", 7) == 0) {
        const char *response = "HTTP/1.1 200 Connection Established\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
    } else {
        // proxy the original request
        send(target_sock, buffer, bytes_read, 0);
    }


    // bidirectional data forwarding
    forward_data(target_sock, client_socket);

    close(target_sock);
    close(client_socket);
    return (void *)return_value;
}

int forward_data(int from_sock, int to_sock) {
    char buffer[BUFFER_SIZE];
    ssize_t len;
    // 数据转发
    fd_set fds;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(to_sock, &fds);
        FD_SET(from_sock, &fds);
        
        if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(to_sock, &fds)) {
            len = recv(to_sock, buffer, BUFFER_SIZE, 0);
            if (len <= 0) break;
            send(from_sock, buffer, len, 0);
        }

        if (FD_ISSET(from_sock, &fds)) {
            len = recv(from_sock, buffer, BUFFER_SIZE, 0);
            if (len <= 0) break;
            send(to_sock, buffer, len, 0);
        }
    }
}

int init_forward_socket(const char *host, int port) {
    // 创建目标套接字
    int target_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in target_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (inet_pton(AF_INET, host, &target_addr.sin_addr) <= 0) {
        struct hostent *hostent = gethostbyname(host);
        if (!hostent) {
            perror("Invalid address");
            return -1;
        }
        memcpy(&target_addr.sin_addr, hostent->h_addr, hostent->h_length);
    }

    // 连接目标服务器
    if (connect(target_sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
        perror("Connect failed");
        return -1;
    }

    return target_sock;
}
