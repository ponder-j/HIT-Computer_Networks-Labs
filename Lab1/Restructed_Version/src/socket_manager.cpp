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

/**
 * @brief 代理服务器核心线程函数
 * @param lpParameter 线程参数，类型为ProxyParam*，包含客户端和服务器套接字信息
 * @return 线程退出代码（始终返回0）
 * @details 该函数是代理服务器的核心逻辑，负责处理单个客户端连接的完整生命周期，包括：
 *          1. 用户访问权限过滤
 *          2. HTTP请求解析
 *          3. 网站访问过滤
 *          4. 网站引导（钓鱼重定向）
 *          5. 缓存管理（查询、验证、保存）
 *          6. 与目标服务器通信
 *          7. 响应转发和处理
 */
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
    // ========== 第一部分：变量初始化 ==========
    // 数据缓冲区
    char *Buffer = new char[MAXSIZE];              // 通用缓冲区，用于接收和转发数据
    char *ResponseBuffer = new char[MAXSIZE * 10]; // 响应缓冲区，用于存储完整的HTTP响应
    ZeroMemory(Buffer, MAXSIZE);
    ZeroMemory(ResponseBuffer, MAXSIZE * 10);

    // 网络相关变量
    SOCKADDR_IN clientAddr;          // 客户端地址信息
    int length = sizeof(SOCKADDR_IN);
    int recvSize;                    // 每次接收的数据大小
    int ret;                         // 函数返回值
    int totalBytes = 0;              // 总共转发的字节数
    int responseBufferSize = 0;      // 响应缓冲区中已存储的数据大小
    bool headerParsed = false;       // 标记HTTP响应头是否已解析
    
    // HTTP协议相关变量
    HttpHeader* httpHeader = NULL;       // HTTP请求头结构体
    HttpResponse* httpResponse = NULL;   // HTTP响应头结构体
    char *CacheBuffer = NULL;            // 临时缓冲区，用于解析HTTP头
    
    // 缓存相关变量
    char cacheKey[512];              // 缓存键（根据host+port+url生成）
    char *cachedData = NULL;         // 缓存数据指针
    int cachedDataSize = 0;          // 缓存数据大小
    char lastModified[128] = {0};    // Last-Modified时间戳

    // 获取线程参数（包含客户端套接字和客户端IP）
    ProxyParam* param = (ProxyParam*)lpParameter;

    // ========== 第二部分：客户端信息获取 ==========
    // 获取客户端地址信息（IP和端口）
    getpeername(param->clientSocket, (SOCKADDR*)&clientAddr, &length);
    printf("[新连接] 客户端: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    // ========== 第三部分：用户访问权限过滤 ==========
    // 检查客户端IP是否在用户黑名单中
    if (!CheckUserAccess(param->clientIP)) {
        printf("[用户过滤] 阻止用户访问: %s\n", param->clientIP);
        SendBlockedResponse(param->clientSocket);  // 发送403禁止访问响应
        goto error;  // 跳转到清理代码
    }

    // ========== 第四部分：接收并解析HTTP请求 ==========
    // 从客户端接收HTTP请求数据
    recvSize = recv(param->clientSocket, Buffer, MAXSIZE, 0);
    if (recvSize <= 0) {
        printf("[错误] 接收客户端请求失败\n");
        goto error;
    }

    printf("[接收] 收到 %d 字节的HTTP请求\n", recvSize);

    // 解析HTTP请求头部（提取方法、URL、主机名、端口等信息）
    httpHeader = new HttpHeader();
    CacheBuffer = new char[recvSize + 1];  // 创建临时缓冲区用于解析
    ZeroMemory(CacheBuffer, recvSize + 1); // 初始化缓冲区
    memcpy(CacheBuffer, Buffer, recvSize);  // 复制请求数据到临时缓冲区
    ParseHttpHead(CacheBuffer, httpHeader);  // 解析HTTP头部
    delete[] CacheBuffer;
    CacheBuffer = NULL;

    // 验证是否成功解析出目标主机
    if (strlen(httpHeader->host) == 0) {
        printf("[错误] 无法解析目标主机\n");
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    // 忽略HTTPS的CONNECT请求（本代理不支持HTTPS隧道）
    if (strcmp(httpHeader->method, "CONNECT") == 0) {
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[解析] 方法: %s, 主机: %s:%d, URL: %s\n",
           httpHeader->method, httpHeader->host, httpHeader->port, httpHeader->url);

    // ========== 第五部分：网站访问过滤 ==========
    // 检查目标网站是否在黑名单中（根据主机名、端口和URL）
    if (!CheckWebsiteAccess(httpHeader->host, httpHeader->port, httpHeader->url)) {
        printf("[网站过滤] 阻止访问网站: %s:%d%s\n", httpHeader->host, httpHeader->port, httpHeader->url);
        SendBlockedResponse(param->clientSocket);  // 发送403禁止访问响应
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    // ========== 第六部分：网站引导（钓鱼重定向）==========
    // 检查是否需要将请求重定向到其他网站（用于钓鱼演示）
    char redirectHost[1024];
    int redirectPort;
    if (GetRedirectTarget(httpHeader->host, redirectHost, &redirectPort)) {
        printf("[网站引导] %s -> %s:%d\n", httpHeader->host, redirectHost, redirectPort);
        // 修改目标主机和端口，实现透明重定向
        strcpy_s(httpHeader->host, sizeof(httpHeader->host), redirectHost);
        httpHeader->port = redirectPort;
    }

    // ========== 第七部分：缓存处理（仅GET请求）==========
    if (strcmp(httpHeader->method, "GET") == 0) {
        // 生成缓存键（格式：host:port/url）
        GenerateCacheKey(httpHeader->host, httpHeader->port, httpHeader->url, cacheKey);

        // 尝试从缓存中加载数据
        if (LoadCache(cacheKey, &cachedData, &cachedDataSize, lastModified)) {
            printf("[缓存] 找到缓存，大小: %d 字节, Last-Modified: %s\n",
                   cachedDataSize, lastModified);

            // 如果缓存中有Last-Modified信息，则在请求中添加If-Modified-Since头
            // 这样服务器可以判断资源是否被修改，如果未修改则返回304状态码
            if (strlen(lastModified) > 0) {
                ModifyRequestWithCache(Buffer, &recvSize, lastModified);
                printf("[缓存] 添加 If-Modified-Since: %s\n", lastModified);
            }
        } else {
            printf("[缓存] 未找到缓存\n");
        }
    }

    // ========== 第八部分：连接目标服务器 ==========
    // 建立到目标服务器的TCP连接
    if (!ConnectToServer(&param->serverSocket, httpHeader->host, httpHeader->port)) {
        printf("[错误] 连接目标服务器 %s:%d 失败\n", httpHeader->host, httpHeader->port);

        // 如果连接失败但有缓存数据，则直接返回缓存数据
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

    // ========== 第九部分：转发客户端请求到目标服务器 ==========
    // 将修改后的HTTP请求发送到目标服务器
    ret = send(param->serverSocket, Buffer, recvSize, 0);
    if (ret == SOCKET_ERROR) {
        printf("[错误] 转发请求失败\n");
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[转发] 已转发请求到目标服务器 (%d 字节)\n", ret);

    // ========== 第十部分：接收并转发服务器响应 ==========
    // 循环接收目标服务器的响应数据
    totalBytes = 0;
    responseBufferSize = 0;
    headerParsed = false;
    httpResponse = new HttpResponse();

    while (true) {
        ZeroMemory(Buffer, MAXSIZE);
        // 从目标服务器接收响应数据
        recvSize = recv(param->serverSocket, Buffer, MAXSIZE, 0);

        // 如果接收失败或连接关闭，退出循环
        if (recvSize <= 0) {
            break;
        }

        totalBytes += recvSize;  // 累计接收的总字节数

        // 将接收到的数据存储到响应缓冲区（用于缓存和解析）
        if (responseBufferSize + recvSize < MAXSIZE * 10) {
            memcpy(ResponseBuffer + responseBufferSize, Buffer, recvSize);
            responseBufferSize += recvSize;
        }

        // 第一次接收到数据时，解析HTTP响应头部
        if (!headerParsed && responseBufferSize > 0) {
            ParseHttpResponse(ResponseBuffer, responseBufferSize, httpResponse);
            headerParsed = true;
            printf("[响应] 状态码: %d\n", httpResponse->statusCode);
            if (httpResponse->hasLastModified) {
                printf("[响应] Last-Modified: %s\n", httpResponse->lastModified);
            }

            // 处理304 Not Modified响应（资源未修改）
            // 如果服务器返回304且本地有缓存，则直接使用缓存数据响应客户端
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

        // 将接收到的数据立即转发给客户端（流式传输）
        ret = send(param->clientSocket, Buffer, recvSize, 0);
        if (ret == SOCKET_ERROR) {
            printf("[错误] 转发响应到客户端失败\n");
            break;
        }
    }

    printf("[完成] 已转发响应到客户端 (总计 %d 字节)\n", totalBytes);

    // ========== 第十一部分：缓存保存和更新 ==========
    // 只对GET请求的响应进行缓存
    if (strcmp(httpHeader->method, "GET") == 0) {
        if (httpResponse->statusCode == 304) {
            // 304响应表示缓存仍然有效，无需更新
            printf("[缓存] 缓存仍然有效\n");
        } else if (httpResponse->statusCode == 200 && responseBufferSize > 0) {
            // 200响应表示获取到新数据，需要保存到缓存
            // 只有包含Last-Modified头的响应才会被缓存（用于后续验证）
            if (httpResponse->hasLastModified) {
                SaveCache(cacheKey, ResponseBuffer, responseBufferSize, httpResponse->lastModified);
                printf("[缓存] 已保存缓存，大小: %d 字节\n", responseBufferSize);
            } else {
                printf("[缓存] 响应无Last-Modified头，不缓存\n");
            }
        }
    }

    // ========== 第十二部分：清理资源（正常退出）==========
    // 释放缓存数据
    if (cachedData) {
        delete[] cachedData;
        cachedData = NULL;
    }
    // 释放HTTP头结构体
    if (httpHeader) {
        delete httpHeader;
        httpHeader = NULL;
    }
    // 释放HTTP响应结构体
    if (httpResponse) {
        delete httpResponse;
        httpResponse = NULL;
    }

error:  // 错误处理标签：统一资源清理入口
    // ========== 第十三部分：错误处理和最终清理 ==========
    // 确保所有动态分配的内存都被释放
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

    // 关闭客户端套接字
    if (param->clientSocket != INVALID_SOCKET) {
        closesocket(param->clientSocket);
    }
    // 关闭服务器套接字
    if (param->serverSocket != INVALID_SOCKET) {
        closesocket(param->serverSocket);
    }

    // 释放线程参数
    delete param;
    
    // 结束线程
    _endthreadex(0);
    return 0;
}
