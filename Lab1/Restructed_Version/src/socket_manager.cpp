#include "../include/socket_manager.h"
#include "../include/http_parser.h"
#include "../include/cache_manager.h"
#include "../include/filter_manager.h"
#include <stdio.h>
#include <string.h>
#include <process.h>

// 初始化套接字
BOOL InitSocket(SOCKET *proxyServer, int port) {
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        printf("加载winsock失败,错误代码为: %d\n", WSAGetLastError());
        return FALSE;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("不能找到正确的winsock版本\n");
        WSACleanup();
        return FALSE;
    }

    *proxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == *proxyServer) {
        printf("创建套接字失败,错误代码为:%d\n", WSAGetLastError());
        return FALSE;
    }

    SOCKADDR_IN proxyServerAddr;
    proxyServerAddr.sin_family = AF_INET;
    proxyServerAddr.sin_port = htons(port);
    proxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(*proxyServer, (SOCKADDR*)&proxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        printf("绑定套接字失败\n");
        closesocket(*proxyServer);
        return FALSE;
    }

    if (listen(*proxyServer, SOMAXCONN) == SOCKET_ERROR) {
        printf("监听端口%d失败\n", port);
        closesocket(*proxyServer);
        return FALSE;
    }

    return TRUE;
}

// 连接目标服务器
BOOL ConnectToServer(SOCKET *serverSocket, char *host, int port) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    HOSTENT *hostent = gethostbyname(host);
    if (!hostent) {
        return FALSE;
    }
    in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));

    *serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (*serverSocket == INVALID_SOCKET) {
        return FALSE;
    }

    int timeout = 5000;
    setsockopt(*serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(*serverSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(*serverSocket);
        *serverSocket = INVALID_SOCKET;
        return FALSE;
    }

    return TRUE;
}

// 子线程执行的代理逻辑（核心逻辑 - 带过滤和引导）
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
    char *Buffer = new char[MAXSIZE];
    char *ResponseBuffer = new char[MAXSIZE * 10];
    ZeroMemory(Buffer, MAXSIZE);
    ZeroMemory(ResponseBuffer, MAXSIZE * 10);

    SOCKADDR_IN clientAddr;
    int length = sizeof(SOCKADDR_IN);
    int recvSize;
    int ret;
    int totalBytes = 0;
    int responseBufferSize = 0;
    bool headerParsed = false;
    HttpHeader* httpHeader = NULL;
    HttpResponse* httpResponse = NULL;
    char *CacheBuffer = NULL;
    char cacheKey[512];
    char *cachedData = NULL;
    int cachedDataSize = 0;
    char lastModified[128] = {0};

    ProxyParam* param = (ProxyParam*)lpParameter;

    // 获取客户端地址信息
    getpeername(param->clientSocket, (SOCKADDR*)&clientAddr, &length);
    printf("[新连接] 客户端: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    // ========== 用户过滤检查 ==========
    if (!CheckUserAccess(param->clientIP)) {
        printf("[用户过滤] 阻止用户访问: %s\n", param->clientIP);
        SendBlockedResponse(param->clientSocket);
        goto error;
    }

    // 接收客户端请求
    recvSize = recv(param->clientSocket, Buffer, MAXSIZE, 0);
    if (recvSize <= 0) {
        printf("[错误] 接收客户端请求失败\n");
        goto error;
    }

    printf("[接收] 收到 %d 字节的HTTP请求\n", recvSize);

    // 解析HTTP头部
    httpHeader = new HttpHeader();
    CacheBuffer = new char[recvSize + 1];
    ZeroMemory(CacheBuffer, recvSize + 1);
    memcpy(CacheBuffer, Buffer, recvSize);
    ParseHttpHead(CacheBuffer, httpHeader);
    delete[] CacheBuffer;
    CacheBuffer = NULL;

    if (strlen(httpHeader->host) == 0) {
        printf("[错误] 无法解析目标主机\n");
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    // 忽略 CONNECT 请求（HTTPS）
    if (strcmp(httpHeader->method, "CONNECT") == 0) {
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[解析] 方法: %s, 主机: %s:%d, URL: %s\n",
           httpHeader->method, httpHeader->host, httpHeader->port, httpHeader->url);

    // ========== 网站过滤检查 ==========
    if (!CheckWebsiteAccess(httpHeader->host, httpHeader->port, httpHeader->url)) {
        printf("[网站过滤] 阻止访问网站: %s:%d%s\n", httpHeader->host, httpHeader->port, httpHeader->url);
        SendBlockedResponse(param->clientSocket);
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    // ========== 网站引导（钓鱼）检查 ==========
    char redirectHost[1024];
    int redirectPort;
    if (GetRedirectTarget(httpHeader->host, redirectHost, &redirectPort)) {
        printf("[网站引导] %s -> %s:%d\n", httpHeader->host, redirectHost, redirectPort);
        // 修改目标主机
        strcpy_s(httpHeader->host, sizeof(httpHeader->host), redirectHost);
        httpHeader->port = redirectPort;
    }

    // ========== 缓存处理（仅GET请求）==========
    if (strcmp(httpHeader->method, "GET") == 0) {
        GenerateCacheKey(httpHeader->host, httpHeader->port, httpHeader->url, cacheKey);

        if (LoadCache(cacheKey, &cachedData, &cachedDataSize, lastModified)) {
            printf("[缓存] 找到缓存，大小: %d 字节, Last-Modified: %s\n",
                   cachedDataSize, lastModified);

            if (strlen(lastModified) > 0) {
                ModifyRequestWithCache(Buffer, &recvSize, lastModified);
                printf("[缓存] 添加 If-Modified-Since: %s\n", lastModified);
            }
        } else {
            printf("[缓存] 未找到缓存\n");
        }
    }

    // 连接目标服务器
    if (!ConnectToServer(&param->serverSocket, httpHeader->host, httpHeader->port)) {
        printf("[错误] 连接目标服务器 %s:%d 失败\n", httpHeader->host, httpHeader->port);

        if (cachedData != NULL) {
            printf("[缓存] 服务器连接失败，返回缓存数据\n");
            send(param->clientSocket, cachedData, cachedDataSize, 0);
            delete[] cachedData;
            cachedData = NULL;
        }

        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[连接] 成功连接到目标服务器: %s:%d\n", httpHeader->host, httpHeader->port);

    // 转发客户端请求到目标服务器
    ret = send(param->serverSocket, Buffer, recvSize, 0);
    if (ret == SOCKET_ERROR) {
        printf("[错误] 转发请求失败\n");
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[转发] 已转发请求到目标服务器 (%d 字节)\n", ret);

    // 循环接收目标服务器响应
    totalBytes = 0;
    responseBufferSize = 0;
    headerParsed = false;
    httpResponse = new HttpResponse();

    while (true) {
        ZeroMemory(Buffer, MAXSIZE);
        recvSize = recv(param->serverSocket, Buffer, MAXSIZE, 0);

        if (recvSize <= 0) {
            break;
        }

        totalBytes += recvSize;

        if (responseBufferSize + recvSize < MAXSIZE * 10) {
            memcpy(ResponseBuffer + responseBufferSize, Buffer, recvSize);
            responseBufferSize += recvSize;
        }

        if (!headerParsed && responseBufferSize > 0) {
            ParseHttpResponse(ResponseBuffer, responseBufferSize, httpResponse);
            headerParsed = true;
            printf("[响应] 状态码: %d\n", httpResponse->statusCode);
            if (httpResponse->hasLastModified) {
                printf("[响应] Last-Modified: %s\n", httpResponse->lastModified);
            }

            // 如果是304响应且有缓存，直接发送缓存，不再转发服务器响应
            if (httpResponse->statusCode == 304 && cachedData != NULL && cachedDataSize > 0) {
                printf("[缓存] 服务器返回304，使用缓存替代响应\n");
                ret = send(param->clientSocket, cachedData, cachedDataSize, 0);
                if (ret == SOCKET_ERROR) {
                    printf("[错误] 发送缓存数据失败\n");
                } else {
                    printf("[缓存] 成功发送缓存数据到客户端，大小: %d 字节\n", cachedDataSize);
                }
                // 跳出循环，不再转发服务器的304响应
                break;
            }
        }

        ret = send(param->clientSocket, Buffer, recvSize, 0);
        if (ret == SOCKET_ERROR) {
            printf("[错误] 转发响应到客户端失败\n");
            break;
        }
    }

    printf("[完成] 已转发响应到客户端 (总计 %d 字节)\n", totalBytes);

    // 缓存处理
    if (strcmp(httpHeader->method, "GET") == 0) {
        if (httpResponse->statusCode == 304) {
            // 304已在上面处理，这里只记录日志
            printf("[缓存] 缓存仍然有效\n");
        } else if (httpResponse->statusCode == 200 && responseBufferSize > 0) {
            if (httpResponse->hasLastModified) {
                SaveCache(cacheKey, ResponseBuffer, responseBufferSize, httpResponse->lastModified);
                printf("[缓存] 已保存缓存，大小: %d 字节\n", responseBufferSize);
            } else {
                printf("[缓存] 响应无Last-Modified头，不缓存\n");
            }
        }
    }

    // 清理
    if (cachedData) {
        delete[] cachedData;
        cachedData = NULL;
    }
    if (httpHeader) {
        delete httpHeader;
        httpHeader = NULL;
    }
    if (httpResponse) {
        delete httpResponse;
        httpResponse = NULL;
    }

error:
    if (httpHeader) {
        delete httpHeader;
    }
    if (httpResponse) {
        delete httpResponse;
    }
    if (cachedData) {
        delete[] cachedData;
    }
    delete[] Buffer;
    delete[] ResponseBuffer;

    if (param->clientSocket != INVALID_SOCKET) {
        closesocket(param->clientSocket);
    }
    if (param->serverSocket != INVALID_SOCKET) {
        closesocket(param->serverSocket);
    }

    delete param;
    _endthreadex(0);
    return 0;
}
