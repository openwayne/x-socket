#include "hook.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t target_pid = atoi(argv[1]);

    // attach to target process
    if (attach_to_process(target_pid) == -1) {
        return 1;
    }

    printf("Attached to process %d, tracing syscalls...\n", target_pid);

    // trace syscalls
    trace_syscalls(target_pid);

    // detach from target process
    detach_from_process(target_pid);

    return 0;
}
