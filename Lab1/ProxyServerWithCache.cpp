#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <sys/stat.h>
#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 // 发送数据报文的最大长度
#define HTTP_PORT 80  // HTTP服务器端口
#define CACHE_DIR ".\\cache" // 缓存目录

// HTTP重要头部数据结构
struct HttpHeader {
    char method[8];        // POST/GET/CONNECT
    char url[1024];        // 请求的URL
    char host[1024];       // 目标主机
    int port;              // 目标端口
    char cookie[1024 * 10];// Cookie
    HttpHeader() {
        ZeroMemory(this, sizeof(HttpHeader));
        port = 80;         // 默认端口为80
    }
};

// HTTP响应头部数据结构
struct HttpResponse {
    int statusCode;              // 状态码
    char lastModified[128];      // Last-Modified时间
    char contentType[128];       // Content-Type
    int contentLength;           // Content-Length
    bool hasLastModified;        // 是否有Last-Modified字段
    HttpResponse() {
        ZeroMemory(this, sizeof(HttpResponse));
        statusCode = 0;
        contentLength = -1;
        hasLastModified = false;
    }
};

// 缓存项结构
struct CacheItem {
    char url[2048];              // 完整URL
    char lastModified[128];      // Last-Modified时间
    char filePath[512];          // 缓存文件路径
    int dataSize;                // 数据大小
    time_t cacheTime;            // 缓存时间
};

// 函数声明
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader *httpHeader);
void ParseHttpResponse(char *buffer, int bufferSize, HttpResponse *httpResponse);
BOOL ConnectToServer(SOCKET *serverSocket, char *host, int port);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
void GenerateCacheKey(const char *host, int port, const char *url, char *cacheKey);
BOOL LoadCache(const char *cacheKey, char **data, int *dataSize, char *lastModified);
BOOL SaveCache(const char *cacheKey, const char *data, int dataSize, const char *lastModified);
void ModifyRequestWithCache(char *request, int *requestSize, const char *lastModified);
void InitCacheDirectory();

// 代理相关参数
SOCKET ProxyServer;                  // 代理服务器套接字
sockaddr_in ProxyServerAddr;         // 代理服务器地址结构
const int ProxyPort = 10240;         // 代理服务器监听端口（按实验要求使用10240）

// 代理参数结构（传递给子线程）
struct ProxyParam {
    SOCKET clientSocket;  // 客户端套接字
    SOCKET serverSocket;  // 目标服务器套接字
};

int main()
{
    printf("========================================\n");
    printf("   HTTP 代理服务器 v2.0 (带缓存)\n");
    printf("========================================\n");
    printf("代理服务器正在启动...\n");

    // 初始化缓存目录
    InitCacheDirectory();

    printf("初始化Socket...\n");

    if (!InitSocket()) {
        printf("Socket初始化失败\n");
        return -1;
    }

    printf("代理服务器启动成功!\n");
    printf("监听端口: %d\n", ProxyPort);
    printf("缓存目录: %s\n", CACHE_DIR);
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

// 初始化缓存目录
void InitCacheDirectory() {
    struct stat info;
    if (stat(CACHE_DIR, &info) != 0) {
        // 目录不存在，创建它
        _mkdir(CACHE_DIR);
        printf("[缓存] 创建缓存目录: %s\n", CACHE_DIR);
    }
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

// 子线程执行的代理逻辑（核心逻辑 - 带缓存）
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
    char *Buffer = new char[MAXSIZE];
    char *ResponseBuffer = new char[MAXSIZE * 10]; // 用于存储完整响应
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

    // ========== 检测并忽略 CONNECT 请求（HTTPS）==========
    if (strcmp(httpHeader->method, "CONNECT") == 0) {
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    printf("[解析] 方法: %s, 主机: %s:%d, URL: %s\n",
           httpHeader->method, httpHeader->host, httpHeader->port, httpHeader->url);

    // ========== 仅对GET请求使用缓存 ==========
    if (strcmp(httpHeader->method, "GET") == 0) {
        // 生成缓存键
        GenerateCacheKey(httpHeader->host, httpHeader->port, httpHeader->url, cacheKey);

        // 尝试加载缓存
        if (LoadCache(cacheKey, &cachedData, &cachedDataSize, lastModified)) {
            printf("[缓存] 找到缓存，大小: %d 字节, Last-Modified: %s\n",
                   cachedDataSize, lastModified);

            // 修改请求添加 If-Modified-Since 头
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

        // 如果有缓存，返回缓存
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
            break; // 服务器关闭连接或发生错误
        }

        totalBytes += recvSize;

        // 保存完整响应（用于缓存）
        if (responseBufferSize + recvSize < MAXSIZE * 10) {
            memcpy(ResponseBuffer + responseBufferSize, Buffer, recvSize);
            responseBufferSize += recvSize;
        }

        // 解析响应头（仅第一次）
        if (!headerParsed && responseBufferSize > 0) {
            ParseHttpResponse(ResponseBuffer, responseBufferSize, httpResponse);
            headerParsed = true;
            printf("[响应] 状态码: %d\n", httpResponse->statusCode);
            if (httpResponse->hasLastModified) {
                printf("[响应] Last-Modified: %s\n", httpResponse->lastModified);
            }
        }

        // 转发响应到客户端
        ret = send(param->clientSocket, Buffer, recvSize, 0);
        if (ret == SOCKET_ERROR) {
            printf("[错误] 转发响应到客户端失败\n");
            break;
        }
    }

    printf("[完成] 已转发响应到客户端 (总计 %d 字节)\n", totalBytes);

    // ========== 处理缓存 ==========
    if (strcmp(httpHeader->method, "GET") == 0) {
        if (httpResponse->statusCode == 304) {
            // 304 Not Modified - 使用缓存
            printf("[缓存] 服务器返回304 Not Modified，缓存仍然有效\n");
        } else if (httpResponse->statusCode == 200 && responseBufferSize > 0) {
            // 200 OK - 保存新缓存
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

// 错误处理与资源释放
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

// 解析HTTP请求头部
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
            char *host_start = p + 6;
            while (*host_start == ' ') host_start++;

            // 解析主机名和端口号
            char *colon = strchr(host_start, ':');
            if (colon != NULL) {
                int host_len = colon - host_start;
                if (host_len < 1024) {
                    memcpy(httpHeader->host, host_start, host_len);
                    httpHeader->host[host_len] = '\0';
                }
                httpHeader->port = atoi(colon + 1);
            } else {
                strcpy_s(httpHeader->host, sizeof(httpHeader->host), host_start);
                httpHeader->port = 80;
            }
        }
        else if (strncmp(p, "Cookie:", 7) == 0) {
            char *cookie_start = p + 8;
            while (*cookie_start == ' ') cookie_start++;
            strcpy_s(httpHeader->cookie, sizeof(httpHeader->cookie), cookie_start);
        }
        p = strtok_s(NULL, delim, &ptr);
    }
}

// 解析HTTP响应头部
void ParseHttpResponse(char *buffer, int bufferSize, HttpResponse *httpResponse) {
    char *headerEnd = strstr(buffer, "\r\n\r\n");
    if (headerEnd == NULL) return;

    char *tempBuffer = new char[headerEnd - buffer + 1];
    memcpy(tempBuffer, buffer, headerEnd - buffer);
    tempBuffer[headerEnd - buffer] = '\0';

    char *p;
    char *ptr;
    const char *delim = "\r\n";

    // 解析状态行
    p = strtok_s(tempBuffer, delim, &ptr);
    if (p != NULL) {
        // HTTP/1.1 200 OK
        char *statusStart = strchr(p, ' ');
        if (statusStart != NULL) {
            httpResponse->statusCode = atoi(statusStart + 1);
        }
    }

    // 解析头部字段
    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        if (strncmp(p, "Last-Modified:", 14) == 0) {
            char *value = p + 15;
            while (*value == ' ') value++;
            strcpy_s(httpResponse->lastModified, sizeof(httpResponse->lastModified), value);
            httpResponse->hasLastModified = true;
        }
        else if (strncmp(p, "Content-Type:", 13) == 0) {
            char *value = p + 14;
            while (*value == ' ') value++;
            strcpy_s(httpResponse->contentType, sizeof(httpResponse->contentType), value);
        }
        else if (strncmp(p, "Content-Length:", 15) == 0) {
            char *value = p + 16;
            while (*value == ' ') value++;
            httpResponse->contentLength = atoi(value);
        }
        p = strtok_s(NULL, delim, &ptr);
    }

    delete[] tempBuffer;
}

// 函数：连接目标服务器
BOOL ConnectToServer(SOCKET *serverSocket, char *host, int port) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // DNS 解析
    HOSTENT *hostent = gethostbyname(host);
    if (!hostent) {
        return FALSE;
    }
    in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));

    // 创建连接目标服务器的 socket
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

// 生成缓存键（使用简单的哈希）
void GenerateCacheKey(const char *host, int port, const char *url, char *cacheKey) {
    char fullUrl[2048];
    sprintf_s(fullUrl, sizeof(fullUrl), "%s:%d%s", host, port, url);

    // 简单哈希：使用字符串内容计算
    unsigned int hash = 0;
    for (int i = 0; fullUrl[i] != '\0'; i++) {
        hash = hash * 31 + fullUrl[i];
    }

    sprintf_s(cacheKey, 512, "%s\\%u.cache", CACHE_DIR, hash);
}

// 加载缓存
BOOL LoadCache(const char *cacheKey, char **data, int *dataSize, char *lastModified) {
    // 打开缓存文件
    FILE *file = NULL;
    fopen_s(&file, cacheKey, "rb");
    if (file == NULL) {
        return FALSE;
    }

    // 读取Last-Modified（前128字节）
    char lm[128] = {0};
    fread(lm, 1, 128, file);
    strcpy_s(lastModified, 128, lm);

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 128, SEEK_SET); // 跳过Last-Modified

    // 读取数据
    int contentSize = fileSize - 128;
    *data = new char[contentSize];
    *dataSize = fread(*data, 1, contentSize, file);

    fclose(file);
    return TRUE;
}

// 保存缓存
BOOL SaveCache(const char *cacheKey, const char *data, int dataSize, const char *lastModified) {
    FILE *file = NULL;
    fopen_s(&file, cacheKey, "wb");
    if (file == NULL) {
        return FALSE;
    }

    // 写入Last-Modified（固定128字节）
    char lm[128] = {0};
    strcpy_s(lm, sizeof(lm), lastModified);
    fwrite(lm, 1, 128, file);

    // 写入数据
    fwrite(data, 1, dataSize, file);

    fclose(file);
    return TRUE;
}

// 修改请求添加If-Modified-Since头
void ModifyRequestWithCache(char *request, int *requestSize, const char *lastModified) {
    // 找到请求头结束位置（\r\n\r\n）
    char *headerEnd = strstr(request, "\r\n\r\n");
    if (headerEnd == NULL) return;

    // 检查是否已经有If-Modified-Since头
    if (strstr(request, "If-Modified-Since:") != NULL) {
        return; // 已存在，不重复添加
    }

    // 构建新的If-Modified-Since头
    char ifModifiedSince[256];
    sprintf_s(ifModifiedSince, sizeof(ifModifiedSince), "If-Modified-Since: %s\r\n", lastModified);

    // 计算新请求的大小
    int headerLength = headerEnd - request;
    int newSize = headerLength + strlen(ifModifiedSince) + 4; // +4 for \r\n\r\n

    // 插入If-Modified-Since头
    char *newRequest = new char[MAXSIZE];
    memcpy(newRequest, request, headerLength + 2); // 复制到最后一个\r\n
    strcpy_s(newRequest + headerLength + 2, MAXSIZE - headerLength - 2, ifModifiedSince);
    strcat_s(newRequest, MAXSIZE, "\r\n");

    // 复制body（如果有）
    int bodyLength = *requestSize - (headerEnd - request) - 4;
    if (bodyLength > 0) {
        memcpy(newRequest + newSize, headerEnd + 4, bodyLength);
        newSize += bodyLength;
    }

    // 复制回原buffer
    memcpy(request, newRequest, newSize);
    *requestSize = newSize;

    delete[] newRequest;
}
