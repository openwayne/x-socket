#include "network.h"
#include <stdio.h>
#include <string.h>
#include "queue.h"

int main()
{
    socket_t s;
    initQueue(&s.recv_queue);
    initQueue(&s.send_queue);

    const char *target_host = "www.baidu.com";
    int target_port = 80;
    socket_init(&s, "127.0.0.1", 7778, 1);
    socket_start_recv(&s);
    socket_start_send(&s);

    char send_data[] = "Hello, world!";
    socket_send(&s, send_data, strlen(send_data), target_host, target_port);

    // Wait for the recv thread to finish
    pthread_join(s.recv_thread, NULL);
    pthread_join(s.send_thread, NULL);

    return 0;
}
