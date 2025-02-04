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
#else
int (*originalConnect)(int, const struct sockaddr *, socklen_t) = NULL;
ssize_t (*originalSend)(int, const void *, size_t, int) = NULL;

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
#endif


#endif // __HOOK_H__
