#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <time.h>
#include <map>
#include <string>
#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 // 发送数据报文的最大长度
#define HTTP_PORT 80  // HTTP服务器端口
#define CACHE_DIR "cache\\" // 缓存目录

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

// 缓存信息结构
struct CacheInfo {
    std::string filename;      // 缓存文件名
    time_t lastModified;       // 最后修改时间
    std::string etag;          // ETag
    time_t cachedTime;         // 缓存时间
    int size;                  // 文件大小
};

// 全局缓存映射表（URL -> CacheInfo）
std::map<std::string, CacheInfo> cacheMap;
CRITICAL_SECTION cacheLock; // 缓存锁，用于多线程同步

// 函数声明
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader *httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
void InitCache();
std::string GetCacheFileName(const char* url);
BOOL LoadFromCache(const char* url, char** buffer, int* size, std::string& lastModified);
void SaveToCache(const char* url, const char* buffer, int size, const char* lastModified);
void AddIfModifiedSinceHeader(char* buffer, int* size, const char* lastModified);
std::string ExtractLastModified(const char* response, int size);
BOOL IsNotModified(const char* response);

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
    printf("   HTTP 代理服务器 v2.0 (带缓存)\n");
    printf("========================================\n");
    printf("代理服务器正在启动...\n");

    // 初始化缓存
    InitCache();

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
    DeleteCriticalSection(&cacheLock);
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
    // 协议族：IPv4  类型：TCP   协议：默认
    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == ProxyServer) {
        printf("创建套接字失败,错误代码为:%d\n", WSAGetLastError());
        return FALSE;
    }

    // 配置服务器地址
    ProxyServerAddr.sin_family = AF_INET; // 使用 IPv4 地址
    ProxyServerAddr.sin_port = htons(ProxyPort); // 监听端口
    ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY; // 监听所有可用接口

    // 绑定套接字
    // (SOCKADDR*)& 将 sockaddr_in 结构体强制转换为 SOCKADDR 结构体
    // 提取一个包含 IPv4 地址、端口、监听范围 配置信息的结构体指针
    // 绑定到 ProxyServer 套接字上
    if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        printf("绑定套接字失败\n");
        closesocket(ProxyServer);
        return FALSE;
    }

    // 开始监听
    // SOMAXCONN 指定最大连接队列长度
    if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
        printf("监听端口%d失败\n", ProxyPort);
        closesocket(ProxyServer);
        return FALSE;
    }

    return TRUE;
}

// 子线程执行的代理逻辑（核心逻辑）
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

    // 预先声明所有变量，避免 goto 跳过初始化
    char *ResponseBuffer = NULL;
    BOOL firstPacket = TRUE;
    BOOL notModified = FALSE;

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

    printf("[解析] 方法: %s, 主机: %s, URL: %s\n", httpHeader->method, httpHeader->host, httpHeader->url);

    // ========== 忽略 CONNECT 请求（HTTPS）==========
    // 实验要求只实现 HTTP 代理，不支持 HTTPS
    if (strcmp(httpHeader->method, "CONNECT") == 0) {
        printf("[忽略] CONNECT 请求不支持（仅支持 HTTP）\n");
        delete httpHeader;
        httpHeader = NULL;
        goto error;
    }

    // ========== 缓存检查（仅对 GET 请求）==========
    if (strcmp(httpHeader->method, "GET") == 0) {
        char* cachedData = NULL;
        int cachedSize = 0;
        std::string lastModified;
        std::string fullUrl = std::string(httpHeader->host) + httpHeader->url;

        if (LoadFromCache(fullUrl.c_str(), &cachedData, &cachedSize, lastModified)) {
            printf("[缓存] 找到缓存，验证新鲜度...\n");

            // 添加 If-Modified-Since 头部
            if (!lastModified.empty()) {
                AddIfModifiedSinceHeader(Buffer, &recvSize, lastModified.c_str());
            }
        }
    }

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

    // ========== 接收响应并处理缓存 ==========
    ResponseBuffer = new char[MAXSIZE * 10]; // 更大的缓冲区用于累积响应
    totalBytes = 0;
    firstPacket = TRUE;
    notModified = FALSE;

    while (true) {
        ZeroMemory(Buffer, MAXSIZE);
        recvSize = recv(param->serverSocket, Buffer, MAXSIZE, 0);

        if (recvSize <= 0) {
            break; // 服务器关闭连接或发生错误
        }

        // 检查是否 304 Not Modified
        if (firstPacket) {
            firstPacket = FALSE;
            if (IsNotModified(Buffer)) {
                notModified = TRUE;
                printf("[缓存] 服务器返回304，使用缓存\n");

                // 从缓存读取并发送
                char* cachedData = NULL;
                int cachedSize = 0;
                std::string lastMod;
                std::string fullUrl = std::string(httpHeader->host) + httpHeader->url;

                if (LoadFromCache(fullUrl.c_str(), &cachedData, &cachedSize, lastMod)) {
                    send(param->clientSocket, cachedData, cachedSize, 0);
                    delete[] cachedData;
                }
                break;
            }
        }

        // 累积响应数据（用于保存缓存）
        if (totalBytes + recvSize < MAXSIZE * 10) {
            memcpy(ResponseBuffer + totalBytes, Buffer, recvSize);
            totalBytes += recvSize;
        }

        // 转发响应到客户端
        ret = send(param->clientSocket, Buffer, recvSize, 0);
        if (ret == SOCKET_ERROR) {
            printf("[错误] 转发响应到客户端失败\n");
            break;
        }
    }

    // ========== 保存到缓存（仅 GET 请求且非 304）==========
    if (!notModified && strcmp(httpHeader->method, "GET") == 0 && totalBytes > 0) {
        std::string lastMod = ExtractLastModified(ResponseBuffer, totalBytes);
        std::string fullUrl = std::string(httpHeader->host) + httpHeader->url;
        SaveToCache(fullUrl.c_str(), ResponseBuffer, totalBytes, lastMod.c_str());
    }

    printf("[完成] 已转发响应到客户端 (总计 %d 字节)\n", totalBytes);

    delete[] ResponseBuffer;
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
    if (ResponseBuffer) {
        delete[] ResponseBuffer;
    }

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

// 解析HTTP头部
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
    // 配置目标服务器地址和端口
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);

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

// ==================== 缓存功能实现 ====================

// 初始化缓存系统
void InitCache() {
    InitializeCriticalSection(&cacheLock);
    // 创建缓存目录
    CreateDirectoryA(CACHE_DIR, NULL);
    printf("缓存系统初始化完成\n");
}

// 生成缓存文件名（使用简单的哈希算法）
std::string GetCacheFileName(const char* url) {
    unsigned long hash = 5381;
    const char* str = url;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    char filename[256];
    sprintf_s(filename, "%s%lu.cache", CACHE_DIR, hash);
    return std::string(filename);
}

// 从缓存加载
BOOL LoadFromCache(const char* url, char** buffer, int* size, std::string& lastModified) {
    EnterCriticalSection(&cacheLock);

    auto it = cacheMap.find(url);
    if (it == cacheMap.end()) {
        LeaveCriticalSection(&cacheLock);
        return FALSE;
    }

    CacheInfo& info = it->second;
    FILE* file;
    if (fopen_s(&file, info.filename.c_str(), "rb") != 0) {
        LeaveCriticalSection(&cacheLock);
        return FALSE;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *buffer = new char[*size];
    fread(*buffer, 1, *size, file);
    fclose(file);

    lastModified = info.etag.empty() ? "" : info.etag;

    LeaveCriticalSection(&cacheLock);
    return TRUE;
}

// 保存到缓存
void SaveToCache(const char* url, const char* buffer, int size, const char* lastModified) {
    EnterCriticalSection(&cacheLock);

    std::string filename = GetCacheFileName(url);
    FILE* file;
    if (fopen_s(&file, filename.c_str(), "wb") != 0) {
        LeaveCriticalSection(&cacheLock);
        return;
    }

    fwrite(buffer, 1, size, file);
    fclose(file);

    CacheInfo info;
    info.filename = filename;
    info.cachedTime = time(NULL);
    info.size = size;
    info.etag = lastModified ? lastModified : "";

    cacheMap[url] = info;

    LeaveCriticalSection(&cacheLock);
    printf("[缓存] 已保存: %s (%d 字节)\n", url, size);
}

// 添加 If-Modified-Since 头部
void AddIfModifiedSinceHeader(char* buffer, int* size, const char* lastModified) {
    if (lastModified == NULL || strlen(lastModified) == 0) {
        return;
    }

    // 查找头部结束位置 \r\n\r\n
    char* headerEnd = strstr(buffer, "\r\n\r\n");
    if (headerEnd == NULL) {
        return;
    }

    // 构造新的头部
    char newHeader[MAXSIZE];
    int headerLen = headerEnd - buffer;
    memcpy(newHeader, buffer, headerLen);

    // 添加 If-Modified-Since
    sprintf_s(newHeader + headerLen, MAXSIZE - headerLen,
              "\r\nIf-Modified-Since: %s\r\n\r\n", lastModified);

    // 复制回原 buffer
    int newSize = strlen(newHeader);
    if (headerEnd + 4 < buffer + *size) {
        // 有 body 数据
        int bodySize = *size - (headerEnd + 4 - buffer);
        memcpy(newHeader + newSize, headerEnd + 4, bodySize);
        newSize += bodySize;
    }

    memcpy(buffer, newHeader, newSize);
    *size = newSize;
}

// 提取 Last-Modified 头部
std::string ExtractLastModified(const char* response, int size) {
    const char* lastModStart = strstr(response, "Last-Modified:");
    if (lastModStart == NULL) {
        lastModStart = strstr(response, "last-modified:");
    }
    if (lastModStart == NULL) {
        return "";
    }

    lastModStart += 14; // 跳过 "Last-Modified:"
    while (*lastModStart == ' ') lastModStart++;

    const char* lastModEnd = strstr(lastModStart, "\r\n");
    if (lastModEnd == NULL) {
        return "";
    }

    return std::string(lastModStart, lastModEnd - lastModStart);
}

// 检查是否为 304 Not Modified 响应
BOOL IsNotModified(const char* response) {
    return strstr(response, "304 Not Modified") != NULL ||
           strstr(response, "304 not modified") != NULL;
}
