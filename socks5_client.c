#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define SOCKS5_IP "127.0.0.1"
#define SOCKS5_PORT 1080
#define TARGET_HOST "example.com"
#define TARGET_PORT 80

void socks5_connect(int sock, const char *host, int port) {
    // 握手阶段
    unsigned char handshake[] = {0x05, 0x01, 0x00}; // VER, NMETHODS, METHODS
    send(sock, handshake, sizeof(handshake), 0);

    // 读取握手响应
    unsigned char handshake_res[2];
    recv(sock, handshake_res, 2, 0);
    if (handshake_res[0] != 0x05 || handshake_res[1] != 0x00) {
        fprintf(stderr, "SOCKS5握手失败\n");
        return;
    }

    // 构造连接请求
    unsigned char request[256] = {
        0x05, // VER
        0x01, // CMD: CONNECT
        0x00, // RSV
        0x03  // ATYP: DOMAINNAME
    };

    size_t host_len = strlen(host);
    request[4] = host_len; // 域名长度
    memcpy(request + 5, host, host_len);
    *(unsigned short*)(request + 5 + host_len) = htons(port);

    // 发送请求
    send(sock, request, 5 + host_len + 2, 0);

    // 读取响应
    unsigned char response[10];
    ssize_t len = recv(sock, response, sizeof(response), 0);
    if (len < 10 || response[1] != 0x00) {
        fprintf(stderr, "连接目标失败\n");
        return;
    }

    // 发送测试HTTP请求
    char http_req[256];
    snprintf(http_req, sizeof(http_req),
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n", host);
    send(sock, http_req, strlen(http_req), 0);

    // 接收响应
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }
}

int main() {
    // 连接SOCKS5代理
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in socks5_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SOCKS5_PORT),
        .sin_addr.s_addr = inet_addr(SOCKS5_IP)
    };

    if (connect(sock, (struct sockaddr*)&socks5_addr, sizeof(socks5_addr)) < 0) {
        perror("连接SOCKS5代理失败");
        return 1;
    }

    socks5_connect(sock, TARGET_HOST, TARGET_PORT);
    close(sock);
    return 0;
}
