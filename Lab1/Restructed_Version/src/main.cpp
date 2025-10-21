#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include "../include/http_structs.h"
#include "../include/socket_manager.h"
#include "../include/cache_manager.h"
#include "../include/filter_manager.h"

// 代理相关参数
const int ProxyPort = 10240;

int main()
{
    printf("========================================\n");
    printf("   HTTP 代理服务器 v3.0 (高级版)\n");
    printf("   支持: 缓存 + 过滤 + 引导\n");
    printf("========================================\n");
    printf("代理服务器正在启动...\n");

    // 初始化缓存目录
    InitCacheDirectory();

    // 加载过滤规则
    LoadFilterRules();

    printf("初始化Socket...\n");

    SOCKET ProxyServer;
    if (!InitSocket(&ProxyServer, ProxyPort)) {
        printf("Socket初始化失败\n");
        return -1;
    }

    printf("代理服务器启动成功!\n");
    printf("监听端口: %d\n", ProxyPort);
    printf("缓存目录: %s\n", CACHE_DIR);
    printf("网站过滤规则: %d 条\n", (int)websiteRules.size());
    printf("用户过滤规则: %d 条\n", (int)userRules.size());
    printf("网站引导规则: %d 条\n", (int)redirectRules.size());
    printf("等待客户端连接...\n");
    printf("========================================\n\n");

    SOCKET acceptSocket = INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE hThread;
    SOCKADDR_IN clientAddr;
    int clientAddrLen = sizeof(SOCKADDR_IN);

    // 代理服务器循环监听
    while (true) {
        acceptSocket = accept(ProxyServer, (SOCKADDR*)&clientAddr, &clientAddrLen);
        if (acceptSocket == INVALID_SOCKET) {
            printf("接受连接失败\n");
            continue;
        }

        lpProxyParam = new ProxyParam;
        if (lpProxyParam == NULL) {
            closesocket(acceptSocket);
            continue;
        }

        lpProxyParam->clientSocket = acceptSocket; // 保存客户端套接字
        lpProxyParam->serverSocket = INVALID_SOCKET; // 初始化服务器套接字为无效
        strcpy_s(lpProxyParam->clientIP, sizeof(lpProxyParam->clientIP), inet_ntoa(clientAddr.sin_addr)); // 保存客户端IP地址

        // 创建子线程处理代理请求
        hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
        if (hThread != NULL) {
            CloseHandle(hThread);
        }
    }

    // 资源释放
    closesocket(ProxyServer);
    WSACleanup();
    return 0;
}
