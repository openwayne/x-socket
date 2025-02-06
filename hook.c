#include "hook.h"

ProxyInfo *globalProxyInfo = NULL;

int (*originalConnect)(int, const struct sockaddr *, socklen_t) = NULL;

void setProxyInfo(const char *host, int port, int type)
{
    globalProxyInfo = (ProxyInfo *)malloc(sizeof(ProxyInfo));
    strcpy(globalProxyInfo->host, host);
    globalProxyInfo->port = port;
    globalProxyInfo->type = type;
}

void unsetProxyInfo()
{
    if (globalProxyInfo != NULL)
    {
        free(globalProxyInfo);
        globalProxyInfo = NULL;
    }
}


#if defined(_WIN32) || defined(_WIN64)
CONNECTPROC originalConnect = connect;

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
            return socks5Connect(sockfd, globalProxyInfo->host, globalProxyInfo->port);
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

// Linux

#endif
