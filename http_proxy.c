#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define PROXY_PORT 8080
#define BUFFER_SIZE 4096

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // 读取请求头
    bytes_read = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_sock);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    // 解析 Host
    char *host_start = strstr(buffer, "Host: ");
    if (!host_start) {
        close(client_sock);
        return NULL;
    }
    host_start += 6;
    char *host_end = strstr(host_start, "\r\n");
    if (!host_end) {
        close(client_sock);
        return NULL;
    }
    *host_end = '\0';

    // 提取端口
    char *port_str = strchr(host_start, ':');
    int port = port_str ? atoi(port_str + 1) : 80;
    char host[256];
    strncpy(host, host_start, port_str ? port_str - host_start : host_end - host_start);
    host[port_str ? port_str - host_start : host_end - host_start] = '\0';

    // 创建目标套接字
    int target_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in target_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (inet_pton(AF_INET, host, &target_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_sock);
        close(target_sock);
        return NULL;
    }

    // 连接目标服务器
    if (connect(target_sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
        perror("Connect failed");
        close(client_sock);
        close(target_sock);
        return NULL;
    }

    // 处理 CONNECT 方法
    if (strncmp(buffer, "CONNECT", 7) == 0) {
        const char *response = "HTTP/1.1 200 Connection Established\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
    } else {
        // 直接转发原始请求
        send(target_sock, buffer, bytes_read, 0);
    }

    // 双向转发数据
    fd_set fds;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_sock, &fds);
        FD_SET(target_sock, &fds);
        
        if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(client_sock, &fds)) {
            bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0) break;
            send(target_sock, buffer, bytes_read, 0);
        }

        if (FD_ISSET(target_sock, &fds)) {
            bytes_read = recv(target_sock, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0) break;
            send(client_sock, buffer, bytes_read, 0);
        }
    }

    close(target_sock);
    close(client_sock);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PROXY_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, 10);
    printf("HTTP Proxy listening on port %d\n", PROXY_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);
    }

    return 0;
}
