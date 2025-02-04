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
    struct sockaddr_in *custom_addr = (struct sockaddr_in *) addr;

    // 修改为自定义地址和端口（比如代理服务器地址）
    inet_pton(AF_INET, "127.0.0.1", &custom_addr->sin_addr);  // 重定向到本地 127.0.0.1
    custom_addr->sin_port = htons(8080);  // 使用自定义端口 8080

    // 调用系统原始的 connect 函数
    return syscall(SYS_connect, sockfd, (struct sockaddr *) custom_addr, addrlen);

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

#endif
