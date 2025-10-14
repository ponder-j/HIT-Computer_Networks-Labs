#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <time.h>
#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 // 发送数据报文的最大长度
#define HTTP_PORT 80  // HTTP服务器端口

// HTTP重要头部数据结构
struct HttpHeader {
    char method[8];        // POST/GET/CONNECT
    char url[1024];        // 请求的URL
    char host[1024];       // 目标主机
    char cookie[1024 * 10];// Cookie
    HttpHeader() {
        ZeroMemory(this, sizeof(HttpHeader));
    }
};

// 函数声明
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader *httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

// 代理相关参数
SOCKET ProxyServer;                  // 代理服务器套接字
sockaddr_in ProxyServerAddr;         // 代理服务器地址结构
const int ProxyPort = 8080;          // 代理服务器监听端口（按实验要求使用8080）

// 代理参数结构（传递给子线程）
struct ProxyParam {
    SOCKET clientSocket;  // 客户端套接字
    SOCKET serverSocket;  // 目标服务器套接字
};

int main()
{
    printf("========================================\n");
    printf("       HTTP 代理服务器 v1.0\n");
    printf("========================================\n");
    printf("代理服务器正在启动...\n");
    printf("初始化Socket...\n");

    if (!InitSocket()) {
        printf("Socket初始化失败\n");
        return -1;
    }

    printf("代理服务器启动成功!\n");
    printf("监听端口: %d\n", ProxyPort);
    printf("等待客户端连接...\n");
    printf("========================================\n\n");

    SOCKET acceptSocket = INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE hThread;

    // 代理服务器循环监听
    while (true) {
        acceptSocket = accept(ProxyServer, NULL, NULL);
        if (acceptSocket == INVALID_SOCKET) {
            printf("接受连接失败\n");
            continue;
        }

        lpProxyParam = new ProxyParam;
        if (lpProxyParam == NULL) {
            closesocket(acceptSocket);
            continue;
        }

        lpProxyParam->clientSocket = acceptSocket;
        lpProxyParam->serverSocket = INVALID_SOCKET;

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

// 函数：初始化套接字
BOOL InitSocket() {
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    // 请求Socket版本2.2
    wVersionRequested = MAKEWORD(2, 2);
    // 加载Socket库
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        printf("加载winsock失败,错误代码为: %d\n", WSAGetLastError());
        return FALSE;
    }

    // 验证版本
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("不能找到正确的winsock版本\n");
        WSACleanup();
        return FALSE;
    }

    // 创建TCP套接字
    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == ProxyServer) {
        printf("创建套接字失败,错误代码为:%d\n", WSAGetLastError());
        return FALSE;
    }

    // 配置服务器地址
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(ProxyPort);
    ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;

    // 绑定套接字
    if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        printf("绑定套接字失败\n");
        closesocket(ProxyServer);
        return FALSE;
    }

    // 开始监听
    if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
        printf("监听端口%d失败\n", ProxyPort);
        closesocket(ProxyServer);
        return FALSE;
    }

    return TRUE;
}

// 函数：子线程执行的代理逻辑
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
    char *Buffer = new char[MAXSIZE];
    ZeroMemory(Buffer, MAXSIZE);

    SOCKADDR_IN clientAddr;
    int length = sizeof(SOCKADDR_IN);
    int recvSize;
    int ret;
    int totalBytes = 0;
    HttpHeader* httpHeader = NULL;
    char *CacheBuffer = NULL;

    ProxyParam* param = (ProxyParam*)lpParameter;

    // 获取客户端地址信息
    getpeername(param->clientSocket, (SOCKADDR*)&clientAddr, &length);
    printf("[新连接] 客户端: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

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

    printf("[解析] 方法: %s, 主机: %s\n", httpHeader->method, httpHeader->host);

    // 连接目标服务器
    if (!ConnectToServer(&param->serverSocket, httpHeader->host)) {
        printf("[错误] 连接目标服务器 %s 失败\n", httpHeader->host);
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[连接] 成功连接到目标服务器: %s\n", httpHeader->host);

    // 转发客户端请求到目标服务器
    ret = send(param->serverSocket, Buffer, recvSize, 0);
    if (ret == SOCKET_ERROR) {
        printf("[错误] 转发请求失败\n");
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[转发] 已转发请求到目标服务器 (%d 字节)\n", ret);

    // 循环接收目标服务器响应并转发给客户端
    totalBytes = 0;
    while (true) {
        ZeroMemory(Buffer, MAXSIZE);
        recvSize = recv(param->serverSocket, Buffer, MAXSIZE, 0);

        if (recvSize <= 0) {
            break; // 服务器关闭连接或发生错误
        }

        totalBytes += recvSize;

        // 转发响应到客户端
        ret = send(param->clientSocket, Buffer, recvSize, 0);
        if (ret == SOCKET_ERROR) {
            printf("[错误] 转发响应到客户端失败\n");
            break;
        }
    }

    printf("[完成] 已转发响应到客户端 (总计 %d 字节)\n", totalBytes);
    if (httpHeader) {
        delete httpHeader;
        httpHeader = NULL;
    }

// 错误处理与资源释放
error:
    printf("[关闭] 关闭连接\n\n");
    if (httpHeader) {
        delete httpHeader;
    }
    delete[] Buffer;

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

// 函数：解析HTTP头部
void ParseHttpHead(char *buffer, HttpHeader *httpHeader) {
    char *p;
    char *ptr;
    const char *delim = "\r\n";

    // 提取请求行（第一行）
    p = strtok_s(buffer, delim, &ptr);
    if (p == NULL) return;

    // 判断请求方法（GET/POST/CONNECT）并提取URL
    if (strncmp(p, "GET", 3) == 0) {
        memcpy(httpHeader->method, "GET", 3);
        // 提取URL: "GET /path HTTP/1.1" -> "/path"
        char *url_start = p + 4;
        char *url_end = strstr(url_start, " HTTP");
        if (url_end != NULL) {
            int url_len = url_end - url_start;
            if (url_len < 1024) {
                memcpy(httpHeader->url, url_start, url_len);
            }
        }
    }
    else if (strncmp(p, "POST", 4) == 0) {
        memcpy(httpHeader->method, "POST", 4);
        char *url_start = p + 5;
        char *url_end = strstr(url_start, " HTTP");
        if (url_end != NULL) {
            int url_len = url_end - url_start;
            if (url_len < 1024) {
                memcpy(httpHeader->url, url_start, url_len);
            }
        }
    }
    else if (strncmp(p, "CONNECT", 7) == 0) {
        memcpy(httpHeader->method, "CONNECT", 7);
    }

    // 提取后续头部字段（Host/Cookie）
    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        if (strncmp(p, "Host:", 5) == 0) {
            // Host字段
            char *host_start = p + 6; // 跳过"Host: "
            // 去除前导空格
            while (*host_start == ' ') host_start++;
            strcpy_s(httpHeader->host, sizeof(httpHeader->host), host_start);
        }
        else if (strncmp(p, "Cookie:", 7) == 0) {
            // Cookie字段
            char *cookie_start = p + 8;
            while (*cookie_start == ' ') cookie_start++;
            strcpy_s(httpHeader->cookie, sizeof(httpHeader->cookie), cookie_start);
        }
        p = strtok_s(NULL, delim, &ptr);
    }
}

// 函数：连接目标服务器
BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);

    // 通过主机名获取IP地址
    HOSTENT *hostent = gethostbyname(host);
    if (!hostent) {
        return FALSE;
    }
    in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));

    // 创建连接目标服务器的套接字
    *serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (*serverSocket == INVALID_SOCKET) {
        return FALSE;
    }

    // 设置超时时间
    int timeout = 5000; // 5秒
    setsockopt(*serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(*serverSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    // 连接目标服务器
    if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(*serverSocket);
        *serverSocket = INVALID_SOCKET;
        return FALSE;
    }

    return TRUE;
}
