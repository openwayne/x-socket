#include "proxy.h"

void *initHttpThread(void *arg)
{
    int *returnValue = malloc(sizeof(int));
    *returnValue = 0;

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(HTTP_PORT),
        .sin_addr.s_addr = INADDR_ANY};

    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        *returnValue = -1;
        return (void *)returnValue;
    }

    listen(serverFd, 10);
    printf("HTTP Proxy listening on port %d\n", HTTP_PORT);

    while (1)
    {
        struct sockaddr_in clientAddr;
        void *handleStatus;
        socklen_t addrLen = sizeof(clientAddr);
        int *clientSock = malloc(sizeof(int));
        *clientSock = accept(serverFd, (struct sockaddr *)&clientAddr, &addrLen);

        pthread_t thread;
        pthread_create(&thread, NULL, handleHttpProxy, clientSock);

        pthread_join(thread, &handleStatus);
        *returnValue = *(int *)handleStatus;
        free(handleStatus);
    }

    return (void *)returnValue;
}

void *initSocks5Thread(void *arg)
{
    int *returnValue = malloc(sizeof(int));
    *returnValue = 0;
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SOCKS5_PORT),
        .sin_addr.s_addr = INADDR_ANY};

    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    if (bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        return NULL;
    }

    listen(serverFd, 10);
    printf("SOCKS5 Proxy listening on port %d\n", SOCKS5_PORT);

    while (1)
    {
        struct sockaddr_in clientAddr;
        void *handleStatus;
        socklen_t addrLen = sizeof(clientAddr);
        int *clientSock = malloc(sizeof(int));
        *clientSock = accept(serverFd, (struct sockaddr *)&clientAddr, &addrLen);

        pthread_t thread;
        pthread_create(&thread, NULL, handleSocks5Proxy, clientSock);

        pthread_join(thread, &handleStatus);
        *returnValue = *(int *)handleStatus;

        free(clientSock);
        free(handleStatus);
    }

    return (void *)returnValue;
}

void *handleSocks5Proxy(void *arg)
{
    int clientSock = *(int *)arg;
    printf("clientSock: %d\n", clientSock);
    unsigned char buffer[BUFFER_SIZE];

    int *returnValue = malloc(sizeof(int));
    *returnValue = 0;

    // read greeting
    ssize_t len = recv(clientSock, buffer, BUFFER_SIZE, 0);
    printf("Received HEX: ");
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\nReceived data length: %ld\n\n\n", len);
    if (len < 3 || buffer[0] != 0x05)
    {
        close(clientSock);
        *returnValue = -1;
        return (void *)returnValue;
    }

    // return no authentication required
    unsigned char authResponse[] = {0x05, 0x00};
    send(clientSock, authResponse, 2, 0);
    // read request
    len = recv(clientSock, buffer, BUFFER_SIZE, 0);

    printf("Received HEX: ");
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\nReceived data length: %ld\n\n\n", len);

    if (len < 7 || buffer[1] != 0x01)
    { 
        // only support connect command
        close(clientSock);
        *returnValue = -1;
        return (void *)returnValue;
    }

    // parse target address
    char host[256];
    int port;
    switch (buffer[3])
    {
    case 0x01: // IPv4
        inet_ntop(AF_INET, &buffer[4], host, INET_ADDRSTRLEN);
        port = ntohs(*(unsigned short *)(buffer + 8));
        break;
    case 0x03: // Domain
        memcpy(host, buffer + 5, buffer[4]);
        host[buffer[4]] = '\0';
        port = ntohs(*(unsigned short *)(buffer + 5 + buffer[4]));
        break;
    default:
        close(clientSock);
        *returnValue = -1;
        return (void *)returnValue;
    }

    // connec to target
    int targetSock = initForwardSocket(host, port);

    if (targetSock < 0)
    {
        close(clientSock);
        close(targetSock);
        *returnValue = -1;
        return (void *)returnValue;
    }

    // response success
    unsigned char response[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    send(clientSock, response, 10, 0);

    // bidirectional data forwarding
    forwardData(targetSock, clientSock);

    close(targetSock);
    close(clientSock);
    return (void *)returnValue;
}

void *handleHttpProxy(void *arg)
{
    int clientSocket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;

    int *returnValue = malloc(sizeof(int));
    *returnValue = 0;

    // 读取请求头
    bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesRead <= 0)
    {
        close(clientSocket);
        *returnValue = -1;
        return (void *)returnValue;
    }
    buffer[bytesRead] = '\0';

    printf("Received data: \n%s\n\n\n", buffer);

    // 解析 Host
    char *hostStart = strstr(buffer, "Host: ");
    if (!hostStart)
    {
        close(clientSocket);
        *returnValue = -1;
        return (void *)returnValue;
    }
    hostStart += 6;
    char *hostEnd = strstr(hostStart, "\r\n");
    if (!hostEnd)
    {
        close(clientSocket);
        *returnValue = -1;
        return (void *)returnValue;
    }
    *hostEnd = '\0';

    // parse host and port
    char *portStr = strchr(hostStart, ':');
    int port = portStr ? atoi(portStr + 1) : 80;
    char host[256];
    strncpy(host, hostStart, portStr ? portStr - hostStart : hostEnd - hostStart);
    host[portStr ? portStr - hostStart : hostEnd - hostStart] = '\0';

    printf("Host: %s, Port: %d\n", host, port);

    // create target socket
    int targetSock = initForwardSocket(host, port);

    if (targetSock < 0)
    {
        close(clientSocket);
        close(targetSock);
        *returnValue = -1;
        return (void *)returnValue;
    }

    // resolve CONNECT request
    if (strncmp(buffer, "CONNECT", 7) == 0)
    {
        const char *response = "HTTP/1.1 200 Connection Established\r\n\r\n";
        send(clientSocket, response, strlen(response), 0);
    }
    else
    {
        // proxy the original request
        send(targetSock, buffer, bytesRead, 0);
    }

    // bidirectional data forwarding
    forwardData(targetSock, clientSocket);

    close(targetSock);
    close(clientSocket);
    return (void *)returnValue;
}

int forwardData(int fromSock, int toSock)
{
    char buffer[BUFFER_SIZE];
    ssize_t len;
    // use select to handle bidirectional data forwarding
    fd_set fds;
    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(toSock, &fds);
        FD_SET(fromSock, &fds);

        if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0)
            break;

        if (FD_ISSET(toSock, &fds))
        {
            len = recv(toSock, buffer, BUFFER_SIZE, 0);
            if (len <= 0)
                break;
            send(fromSock, buffer, len, 0);
        }

        if (FD_ISSET(fromSock, &fds))
        {
            len = recv(fromSock, buffer, BUFFER_SIZE, 0);
            if (len <= 0)
                break;
            send(toSock, buffer, len, 0);
        }
    }

    return 0;
}

int initForwardSocket(const char *host, int port)
{
    // create socket
    int targetSock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in targetAddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)};
    if (inet_pton(AF_INET, host, &targetAddr.sin_addr) <= 0)
    {
        struct hostent *hostent = gethostbyname(host);
        if (!hostent)
        {
            perror("Invalid address");
            return -1;
        }
        memcpy(&targetAddr.sin_addr, hostent->h_addr, hostent->h_length);
    }

    // connect to target
    if (connect(targetSock, (struct sockaddr *)&targetAddr, sizeof(targetAddr)) < 0)
    {
        perror("Connect failed");
        return -1;
    }

    return targetSock;
}

int socks5Connect(int sock, const char *host, int port) {
    // handshake process    
    unsigned char handshake[] = {0x05, 0x01, 0x00}; // VER, NMETHODS, METHODS
    send(sock, handshake, sizeof(handshake), 0);

    // read handshake response
    unsigned char handshake_res[2];
    recv(sock, handshake_res, 2, 0);
    if (handshake_res[0] != 0x05 || handshake_res[1] != 0x00) {
        perror("SOCKS5 handshake error!\n");
        return -1;
    }

    // construct request
    unsigned char request[256] = {
        0x05, // VER
        0x01, // CMD: CONNECT
        0x00, // RSV
        0x03  // ATYP: DOMAINNAME
    };

    size_t host_len = strlen(host);
    request[4] = host_len; // domain name length
    memcpy(request + 5, host, host_len);
    *(unsigned short*)(request + 5 + host_len) = htons(port);

    // send request
    send(sock, request, 5 + host_len + 2, 0);

    // read response
    unsigned char response[10];
    ssize_t len = recv(sock, response, sizeof(response), 0);
    if (len < 10 || response[1] != 0x00) {
        perror("SOCKS5 connect error\n\n");
        return -1;
    }
    return 0;
}
