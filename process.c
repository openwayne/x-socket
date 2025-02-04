#include "process.h"

#if defined(__linux__) || defined(__gnu_linux__) || defined(linux)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#elif defined(__APPLE__) || defined(__MACH__)
#include <stdio.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <stdlib.h>
#endif

LinkedListNode *listProcesses()
{
    LinkedListNode *head = NULL;
#if defined(__linux__) || defined(__gnu_linux__) || defined(linux)

    struct dirent *entry;
    DIR *dp = opendir("/proc");

    if (dp == NULL)
    {
        perror("opendir: /proc");
        return NULL;
    }

    while ((entry = readdir(dp)))
    {
        // /proc 中的进程目录是数字
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0)
        {
            char path[MAX_NAME_LEN];
            char cmdline[MAX_NAME_LEN];
            FILE *fp;

            // 构建 /proc/<PID>/cmdline 文件路径
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);

            // 打开 cmdline 文件获取进程的命令行
            fp = fopen(path, "r");
            if (fp)
            {
                if (fgets(cmdline, sizeof(cmdline), fp) != NULL)
                {
                    Process *process = (Process *)malloc(sizeof(Process));
                    process->pid = atoi(entry->d_name);
                    process->name = strdup(cmdline);
                    insertAtEnd(&head, process);
                }
                fclose(fp);
            }
        }
    }

    closedir(dp);

#elif defined(_WIN32) || defined(_WIN64)

    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
    {
        perror("CreateToolhelp32Snapshot");
        return NULL;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hProcessSnap, &pe32))
    {
        perror("Process32First");
        CloseHandle(hProcessSnap);
        return NULL;
    }

    do
    {
        Process *process = (Process *)malloc(sizeof(Process));
        process->pid = pe32.th32ProcessID;
        process->name = strdup(pe32.szExeFile);
        insertAtEnd(&head, process);
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
#elif defined(__APPLE__) || defined(__MACH__)

    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t miblen = 4;
    size_t size;

    if (sysctl(mib, miblen, NULL, &size, NULL, 0) < 0)
    {
        perror("sysctl");
        return NULL;
    }

    struct kinfo_proc *procs = (struct kinfo_proc *)malloc(size);
    if (sysctl(mib, miblen, procs, &size, NULL, 0) < 0)
    {
        perror("sysctl");
        free(procs);
        return NULL;
    }

    int count = size / sizeof(struct kinfo_proc);
    for (int i = 0; i < count; i++)
    {
        Process *process = (Process *)malloc(sizeof(Process));
        process->pid = procs[i].kp_proc.p_pid;
        process->name = strdup(procs[i].kp_proc.p_comm);
        insertAtEnd(&head, process);
    }
    free(procs);
#endif

    return head;
}
