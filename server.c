// create both http and socks5 proxy server
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "network.h"

#define HTTP_PORT 7777
#define SOCKS5_PORT 7778

void *handle_http_proxy(void *arg);
void *handle_socks5_proxy(void *arg);
int send_socks5_greeting_response(int client_sock);
int process_socks5_request(int client_sock);

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
        printf("\n\n");

        // give a response
        char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello, world!";
        send(new_socket, response, strlen(response), 0);
        printf("response data: %s\n\n\n", response);

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
        printf("new_socket: %d\n", new_socket);

        // Step 1: Receive the SOCKS5 greeting request
        socks5_greeting_request_t greeting_req;
        ssize_t bytes_received = recv(new_socket, &greeting_req, sizeof(greeting_req), 0);
        if (bytes_received < 0) {
            perror("Failed to receive SOCKS5 greeting request");
            close(new_socket);
            continue;
        }

        printf("greeting_req.version: %d\n", greeting_req.version);
        printf("greeting_req.nmethods: %d\n", greeting_req.nmethods);
        printf("greeting_req.methods[0]: %d\n", greeting_req.methods[0]);

        // Step 2: Send SOCKS5 greeting response
        if (send_socks5_greeting_response(new_socket) < 0) {
            close(new_socket);
            continue;
        }

        printf("send_socks5_greeting_response\n");

        // Step 3: Process SOCKS5 connection request
        // if (process_socks5_request(new_socket) < 0) {
        //     close(new_socket);
        //     continue;
        // }
        printf("process_socks5_request start\n");

        char buffer[1024];
        
        do {
            bytes_received = recv(new_socket, &buffer, sizeof(buffer), 0);
            printf("bytes_received: %ld\n", bytes_received);
            printf("buffer: %s\n", buffer);
            printf("buffer by HEX: ");
            for (int i = 0; i < bytes_received; i++) {
                printf("%02X ", buffer[i]);
            }

        } while (bytes_received > 0);

        printf("\n\n");

        printf("process_socks5_request end\n");

        // Close the connection
        close(new_socket);
    }

    close(server_fd);
    return NULL;
}

// Function to send a SOCKS5 greeting response
int send_socks5_greeting_response(int client_sock) {
    socks5_greeting_response_t response;
    response.version = SOCKS5_VERSION;
    response.method = SOCKS5_NO_AUTH; // No authentication required
    
    ssize_t bytes_sent = send(client_sock, &response, sizeof(response), 0);
    if (bytes_sent < 0) {
        perror("Failed to send SOCKS5 greeting response");
        return -1;
    }
    return 0;
}

// Function to process the SOCKS5 request
int process_socks5_request(int client_sock) {
    socks5_request_t request;
    ssize_t bytes_received = recv(client_sock, &request, sizeof(request), 0);
    if (bytes_received < 0) {
        perror("Failed to receive SOCKS5 request");
        return -1;
    }

    printf("request.version: %d\n", request.version);
    printf("request.command: %d\n", request.command);
    printf("request.reserved: %d\n", request.reserved);
    printf("request.address_type: %d\n", request.address_type);
    printf("request.dst_port: %d\n", request.dst_port);
    printf("request.dst_addr.domain: %s\n", request.dst_addr.domain.domain);

    if (request.version != SOCKS5_VERSION) {
        fprintf(stderr, "Invalid SOCKS version\n");
        return -1;
    }

    // Proxy the data between the client and the remote server
    int remote_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_sock < 0) {
        perror("Failed to create remote socket");
        return -1;
    }

    // Connect to the remote server
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(request.dst_port);

    // Check the address type
    if (request.address_type == 0x01) {
        // IPv4 address
        memcpy(&remote_addr.sin_addr.s_addr, request.dst_addr.ipv4.ip, 4);
    } else if (request.address_type == 0x03) {
        // Domain name
        struct hostent *host = gethostbyname(request.dst_addr.domain.domain);

        if (host == NULL) {
            perror("Failed to resolve domain name");
            return -1;
        }
        printf("h_addr_list\n");
        for (int i = 0; host->h_addr_list[i] != NULL; i++) {
            printf("%s\n\n\n", host->h_addr_list[i]);
        }
        memcpy(&remote_addr.sin_addr.s_addr, host->h_addr_list[0], 4);
    } else {
        fprintf(stderr, "Invalid address type\n");
        return -1;
    }

    printf("remote_addr.sin_addr.s_addr: %d\n", remote_addr.sin_addr.s_addr);
    printf("remote_addr.sin_port: %d\n", remote_addr.sin_port);
    printf("remote_addr.sin_family: %d\n", remote_addr.sin_family);
    printf("remote_addr.sin_zero: %s\n", remote_addr.sin_zero);

    if (connect(remote_sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("Failed to connect to remote server\n\n");
        close(remote_sock);
        return -1;
    }

    printf("Connected to remote server\n");

    // Send the SOCKS5 connection reply
    socks5_response_t response;
    response.version = SOCKS5_VERSION;
    response.reply = SOCKS5_SUCCESS;
    response.reserved = 0x00;
    response.address_type = request.address_type;
    response.bnd_port = request.dst_port;

    if (request.address_type == 0x01) {
        // IPv4 address
        memcpy(response.bnd_addr.ipv4.ip, &remote_addr.sin_addr.s_addr, 4);
    } else if (request.address_type == 0x03) {
        // Domain name
        response.bnd_addr.domain.length = strlen(request.dst_addr.domain.domain);
        strncpy(response.bnd_addr.domain.domain, request.dst_addr.domain.domain, response.bnd_addr.domain.length);
    }

    ssize_t bytes_sent = send(client_sock, &response, sizeof(response), 0);
    if (bytes_sent < 0) {
        perror("Failed to send SOCKS5 connection reply");
        close(remote_sock);
        return -1;
    }

    // Proxy the data between the client and the remote server
    char buffer[1024];

    while (1) {
        // Receive data from the client
        ssize_t bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            perror("Failed to receive data from client");
            break;
        } else if (bytes_received == 0) {
            break;
        }

        // Send the data to the remote server
        ssize_t bytes_sent = send(remote_sock, buffer, bytes_received, 0);
        if (bytes_sent < 0) {
            perror("Failed to send data to remote server");
            break;
        } else if (bytes_sent == 0) {
            break;
        }

        // Receive data from the remote server
        bytes_received = recv(remote_sock, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            perror("Failed to receive data from remote server");
            break;
        } else if (bytes_received == 0) {
            break;
        }

        // Send the data to the client
        bytes_sent = send(client_sock, buffer, bytes_received, 0);
        if (bytes_sent < 0) {
            perror("Failed to send data to client");
            break;
        } else if (bytes_sent == 0) {
            break;
        }
    }



    // Close the sockets
    close(remote_sock);
    close(client_sock);
    return 0;
}
