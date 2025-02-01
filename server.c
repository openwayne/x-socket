// create both http and socks5 proxy server
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define HTTP_PORT 7777
#define SOCKS5_PORT 7778

void *handle_http_proxy(void *arg);
void *handle_socks5_proxy(void *arg);

int main() {
    pthread_t http_thread, socks5_thread;

    // Create HTTP proxy thread
    if (pthread_create(&http_thread, NULL, handle_http_proxy, NULL) != 0) {
        perror("Failed to create HTTP proxy thread");
        exit(EXIT_FAILURE);
    }

    // Create SOCKS5 proxy thread
    if (pthread_create(&socks5_thread, NULL, handle_socks5_proxy, NULL) != 0) {
        perror("Failed to create SOCKS5 proxy thread");
        exit(EXIT_FAILURE);
    }

    // Wait for both threads to finish
    pthread_join(http_thread, NULL);
    pthread_join(socks5_thread, NULL);

    return 0;
}

void *handle_http_proxy(void *arg) {
    printf("HTTP proxy server running on port %d\n", HTTP_PORT);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        pthread_exit(NULL);
    }

    // Bind the socket to the HTTP port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        pthread_exit(NULL);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        pthread_exit(NULL);
    }

    while (1) {
        // Accept an incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            close(server_fd);
            pthread_exit(NULL);
        }

        // Read data from the client
        int valread = read(new_socket, buffer, 1024);
        if (valread < 0) {
            perror("read failed");
            close(new_socket);
            continue;
        }

        // Print the received data
        printf("http proxy Received data: %s\n", buffer);

        // Print the received data by HEX
        printf("http proxy Received data by HEX: ");
        for (int i = 0; i < valread; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");

        // give a response
        char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello, world!";
        send(new_socket, response, strlen(response), 0);

        // Close the connection
        close(new_socket);
    }

    close(server_fd);
    return NULL;
}

void *handle_socks5_proxy(void *arg) {
    printf("SOCKS5 proxy server running on port %d\n", SOCKS5_PORT);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        pthread_exit(NULL);
    }

    // Bind the socket to the SOCKS5 port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SOCKS5_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        pthread_exit(NULL);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        pthread_exit(NULL);
    }

    while (1) {
        // Accept an incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            close(server_fd);
            pthread_exit(NULL);
        }

        // Read data from the client
        int valread = read(new_socket, buffer, 1024);
        if (valread < 0) {
            perror("read failed");
            close(new_socket);
            continue;
        }

        // Print the received data
        printf("socks5 proxy Received data: %s\n", buffer);

        // Print the received data by HEX
        printf("socks5 proxy Received data by HEX: ");
        for (int i = 0; i < valread; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");

        // give a response
        char response[] = {0x05, 0x00};
        send(new_socket, response, 2, 0);
        printf("socks5 proxy Response: 0x05 0x00\n");

        // Close the connection
        close(new_socket);
    }

    close(server_fd);
    return NULL;
}
