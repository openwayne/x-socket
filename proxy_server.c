#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "proxy.h"

int main() {
    pthread_t http_thread, socks5_thread;
    void *http_status = NULL, *socks5_status = NULL;

    // Create HTTP proxy thread
    if (pthread_create(&http_thread, NULL, initHttpThread, NULL) != 0) {
        perror("Failed to create HTTP proxy thread");
        exit(EXIT_FAILURE);
    }

    // Create SOCKS5 proxy thread
    if (pthread_create(&socks5_thread, NULL, initSocks5Thread, NULL) != 0) {
        perror("Failed to create SOCKS5 proxy thread");
        exit(EXIT_FAILURE);
    }

    // Wait for both threads to finish
    pthread_join(socks5_thread, socks5_status);
    printf("Socks5 thread finished\n");
    printf("Socks5 thread status: %d\n", *(int *)socks5_status);
    pthread_join(http_thread, http_status);
    printf("Http thread finished\n");
    printf("Http thread status: %d\n", *(int *)http_status);

    free(http_status);
    free(socks5_status);

    return 0;
}
