#ifndef __HOOK_H__
#define __HOOK_H__

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proxy.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <detours.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <sys/user.h>
#endif



typedef struct ProxyInfo
{
    char host[256];
    int port;
    int type;
} ProxyInfo;

ProxyInfo* globalProxyInfo = NULL;

// for client hook
void setProxyInfo(const char *host, int port, int type);
void unsetProxyInfo();

#if defined(_WIN32) || defined(_WIN64)
int InjectDLL(DWORD processId, const char *dllPath);
int EjectDLL(DWORD processId, const char *dllPath);
typedef int (WINAPI *CONNECTPROC)(int, const struct sockaddr *, socklen_t);
CONNECTPROC originalConnect = connect;

int WINAPI proxyConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void HookFunctions();
#else
int (*originalConnect)(int, const struct sockaddr *, socklen_t) = NULL;
ssize_t (*originalSend)(int, const void *, size_t, int) = NULL;

// LD_PRELOAD version
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// ptrace version
#define CONNECT_SYSCALL_NUM SYS_connect
int attach_to_process(pid_t target_pid);
int detach_from_process(pid_t target_pid);
int hook_function(pid_t target_pid, const char *function_name, void *hook_function, void **original_function);
int proxyConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void trace_syscalls(pid_t pid);
void handle_connect(pid_t pid);

#endif


#endif // __HOOK_H__
