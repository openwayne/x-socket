#include "network.h"
#include <stdio.h>
#include <string.h>

int main()
{
    socket_t s;
    const char *target_host = "www.baidu.com";
    int target_port = 80;
    socket_init(&s, "127.0.0.1", 7777, 1);
    socket_start_recv(&s);
    socket_start_send(&s);

    char send_data[] = "Hello, world!";
    socket_send(&s, send_data, strlen(send_data), target_host, target_port);

    char recv_data[BUFFER_SIZE];
    int len = socket_recv(&s, recv_data, BUFFER_SIZE);
    if (len > 0)
    {
        printf("Received: %s\n", recv_data);
    }

    socket_close(&s);   
    return 0;
}
