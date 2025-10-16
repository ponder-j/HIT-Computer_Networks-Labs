#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "http_structs.h"

// 初始化套接字
BOOL InitSocket(SOCKET *proxyServer, int port);

// 连接目标服务器
BOOL ConnectToServer(SOCKET *serverSocket, char *host, int port);

// 代理线程处理函数
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

#endif // SOCKET_MANAGER_H
