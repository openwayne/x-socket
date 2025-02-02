#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define PROXY_IP "127.0.0.1"
#define PROXY_PORT 8080
#define TARGET_HOST "example.com"
#define TARGET_PORT 80

void http_over_proxy(int proxy_sock, const char *host, int port) {
    // 构造 HTTP 请求
    char request[1024];
    snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: C-Proxy-Client/1.0\r\n"
        "Connection: close\r\n\r\n", host);

    send(proxy_sock, request, strlen(request), 0);

    // 接收响应
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = recv(proxy_sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }
}

void https_over_proxy(int proxy_sock, const char *host, int port) {
    // 发送 CONNECT 请求
    char connect_request[256];
    snprintf(connect_request, sizeof(connect_request),
        "CONNECT %s:%d HTTP/1.1\r\n"
        "Host: %s:%d\r\n\r\n", host, port, host, port);
    
    send(proxy_sock, connect_request, strlen(connect_request), 0);

    // 读取 CONNECT 响应
    char response[4096];
    ssize_t len = recv(proxy_sock, response, sizeof(response), 0);
    if (len <= 0 || !strstr(response, "200 Connection Established")) {
        fprintf(stderr, "CONNECT failed\n");
        return;
    }

    // 此处可继续实现 TLS 握手和HTTPS通信
    printf("CONNECT成功，可继续实现TLS通信...\n");
}

int main() {
    // 连接代理服务器
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in proxy_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PROXY_PORT),
        .sin_addr.s_addr = inet_addr(PROXY_IP)
    };

    if (connect(sock, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("连接代理失败");
        return 1;
    }

    printf("选择模式：\n1. HTTP\n2. HTTPS\n> ");
    int choice;
    scanf("%d", &choice);

    if (choice == 1) {
        http_over_proxy(sock, TARGET_HOST, TARGET_PORT);
    } else {
        https_over_proxy(sock, TARGET_HOST, 443);
    }

    close(sock);
    return 0;
}
