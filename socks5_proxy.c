#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>


#define SOCKS5_PORT 1080
#define BUFFER_SIZE 4096

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    unsigned char buffer[BUFFER_SIZE];

    // 握手阶段
    ssize_t len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (len < 3 || buffer[0] != 0x05) {
        close(client_sock);
        return NULL;
    }

    // 返回无需认证
    unsigned char auth_response[] = {0x05, 0x00};
    send(client_sock, auth_response, 2, 0);

    // 读取请求
    len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (len < 7 || buffer[1] != 0x01) { // 只处理 CONNECT
        close(client_sock);
        return NULL;
    }

    // 解析目标地址
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
            return NULL;
    }

    // 连接目标
    int target_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in target_addr = {0};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &target_addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(host);
        if (!he) {
            close(client_sock);
            close(target_sock);
            return NULL;
        }
        memcpy(&target_addr.sin_addr, he->h_addr, he->h_length);
    }

    if (connect(target_sock, (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
        close(client_sock);
        close(target_sock);
        return NULL;
    }

    // 返回成功响应
    unsigned char response[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    send(client_sock, response, 10, 0);

    // 数据转发
    fd_set fds;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_sock, &fds);
        FD_SET(target_sock, &fds);
        
        if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(client_sock, &fds)) {
            len = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if (len <= 0) break;
            send(target_sock, buffer, len, 0);
        }

        if (FD_ISSET(target_sock, &fds)) {
            len = recv(target_sock, buffer, BUFFER_SIZE, 0);
            if (len <= 0) break;
            send(client_sock, buffer, len, 0);
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
        .sin_port = htons(SOCKS5_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, 10);
    printf("SOCKS5 Proxy listening on port %d\n", SOCKS5_PORT);

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
