#include "hook.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t target_pid = atoi(argv[1]);

    // 附加到目标进程
    if (attach_to_process(target_pid) == -1) {
        return 1;
    }

    printf("Attached to process %d, tracing syscalls...\n", target_pid);

    // 开始跟踪系统调用
    trace_syscalls(target_pid);

    // 从目标进程分离
    detach_from_process(target_pid);

    return 0;
}
