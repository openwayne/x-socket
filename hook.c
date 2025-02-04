#include "hook.h"

#if defined(_WIN32) || defined(_WIN64)
int InjectDLL(DWORD processId, const char *dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess)
    {
        printf("Failed to open target process\n");
        return 1;
    }

    void *pLibRemote = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProcess, pLibRemote, (void *)dllPath, strlen(dllPath) + 1, NULL);

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, pLibRemote, 0, NULL);
    if (!hThread)
    {
        printf("Failed to create remote thread\n");
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return 0;
}
int EjectDLL(DWORD processId, const char *dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess)
    {
        printf("Failed to open target process\n");
        return 1;
    }

    HMODULE hModule = GetModuleHandleA("kernel32.dll");
    FARPROC pThreadProc = GetProcAddress(hModule, "FreeLibrary");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pThreadProc, GetModuleHandleA(dllPath), 0, NULL);
    if (!hThread)
    {
        printf("Failed to create remote thread\n");
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return 0;
}

int WINAPI proxyConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    printf("Intercepted connect call\n");

    // Call original connect function
    if (globalProxyInfo != NULL)
    {
        printf("Connecting to %s:%d\n", globalProxyInfo->host, globalProxyInfo->port);
        printf("Connecting type: %d\n", globalProxyInfo->type);
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = htons(globalProxyInfo->port);
        addr_in->sin_addr.s_addr = inet_addr(globalProxyInfo->host);

        int connectRet = originalConnect(sockfd, addr, addrlen);

        if (connectRet < 0)
        {
            return connectRet;
        }

        if (globalProxyInfo->type == 1)
        {
            // SOCKS5 proxy has extra handshake
            return socks5_connect(sockfd, globalProxyInfo->host, globalProxyInfo->port);
        }
    }

    return originalConnect(sockfd, addr, addrlen);
}

void HookFunctions()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID &)originalConnect, proxyConnect);
    DetourTransactionCommit();
}
#else
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    printf("Intercepted connect call\n");

    // Call original connect function
    if (!originalConnect)
    {
        originalConnect = (int (*)(int, const struct sockaddr *, socklen_t))dlsym(RTLD_NEXT, "connect");
    }
    if (globalProxyInfo != NULL)
    {
        printf("Connecting to %s:%d\n", globalProxyInfo->host, globalProxyInfo->port);
        printf("Connecting type: %d\n", globalProxyInfo->type);
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = htons(globalProxyInfo->port);
        addr_in->sin_addr.s_addr = inet_addr(globalProxyInfo->host);

        int connectRet = originalConnect(sockfd, addr, addrlen);

        if (connectRet < 0)
        {
            return connectRet;
        }

        if (globalProxyInfo->type == 1)
        {
            // SOCKS5 proxy has extra handshake
            return socks5_connect(sockfd, globalProxyInfo->host, globalProxyInfo->port);
        }
    }

    return originalConnect(sockfd, addr, addrlen);
}

int proxyConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (globalProxyInfo != NULL)
    {
        printf("Connecting to %s:%d\n", globalProxyInfo->host, globalProxyInfo->port);
        printf("Connecting type: %d\n", globalProxyInfo->type);
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = htons(globalProxyInfo->port);
        addr_in->sin_addr.s_addr = inet_addr(globalProxyInfo->host);

        int connectRet = syscall(SYS_connect, sockfd, addr, addrlen);

        if (connectRet < 0)
        {
            return connectRet;
        }

        if (globalProxyInfo->type == 1)
        {
            // SOCKS5 proxy has extra handshake
            return socks5_connect(sockfd, globalProxyInfo->host, globalProxyInfo->port);
        }
    }
    return syscall(SYS_connect, sockfd, addr, addrlen);
}

// attach to process
int attach_to_process(pid_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        perror("Failed to attach to process");
        return -1;
    }
    waitpid(pid, NULL, 0);
    return 0;
}

// detach from process
int detach_from_process(pid_t pid) {
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) == -1) {
        perror("Failed to detach from process");
        return -1;
    }
    return 0;
}

// hook function
void handle_connect(pid_t pid) {
    struct user_regs_struct regs;

    // get registers
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("Failed to get registers");
        return;
    }

    // check if the syscall is connect
    if (regs.orig_rax == CONNECT_SYSCALL_NUM) {
        struct sockaddr_in addr;
        // peek data to get sockaddr_in
        if (ptrace(PTRACE_PEEKDATA, pid, regs.rsi, &addr) == -1) {
            perror("Failed to peek data");
            return;
        }

        // get host and port
        int sockfd = regs.rdi;
        proxyConnect(sockfd, (struct sockaddr *) &addr, sizeof(addr));

        // poke data to set sockaddr_in
        if (ptrace(PTRACE_POKEDATA, pid, regs.rsi, &addr) == -1) {
            perror("Failed to poke data");
        }
    }
}

// trace syscalls
void trace_syscalls(pid_t pid) {
    int status;
    while (1) {
        // wait for syscall
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
            perror("Failed to trace syscall");
            break;
        }

        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("Process exited\n");
            break;
        }

        handle_connect(pid);
    }
}


#endif
