#include "hook.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <pid> <proxy_host> <proxy_port> <proxy_type>\n", argv[0]);
        return 1;
    }
    setProxyInfo(argv[2], atoi(argv[3]), atoi(argv[4]));

    pid_t target_pid = atoi(argv[1]);

    
    // attach to target process
    if (attach_to_process(target_pid) == -1) {
        return 1;
    }

    printf("Attached to process %d, tracing syscalls...\n", target_pid);

    // trace syscalls
    client_main(globalProxyInfo->host, globalProxyInfo->port, target_pid);

    // detach from target process
    detach_from_process(target_pid);
    unsetProxyInfo();

    return 0;
}
