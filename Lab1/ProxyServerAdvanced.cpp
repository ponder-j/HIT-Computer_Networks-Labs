#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <map>
#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507
#define HTTP_PORT 80
#define CACHE_DIR ".\\cache"
#define BLOCKED_PAGE_HTML "<!DOCTYPE html><html><head><meta charset='utf-8'><title>访问被阻止</title></head><body><h1>403 Forbidden</h1><p>该网站已被管理员阻止访问。</p></body></html>"

// HTTP重要头部数据结构
struct HttpHeader {
    char method[8];
    char url[1024];
    char host[1024];
    int port;
    char cookie[1024 * 10];
    HttpHeader() {
        ZeroMemory(this, sizeof(HttpHeader));
        port = 80;
    }
};

// HTTP响应头部数据结构
struct HttpResponse {
    int statusCode;
    char lastModified[128];
    char contentType[128];
    int contentLength;
    bool hasLastModified;
    HttpResponse() {
        ZeroMemory(this, sizeof(HttpResponse));
        statusCode = 0;
        contentLength = -1;
        hasLastModified = false;
    }
};

// 缓存项结构
struct CacheItem {
    char url[2048];
    char lastModified[128];
    char filePath[512];
    int dataSize;
    time_t cacheTime;
};

// 过滤规则结构
struct FilterRule {
    std::string clientIP;      // 客户端IP（用户过滤）
    std::string hostname;      // 主机名（网站过滤）
    bool isAllowed;            // true=允许，false=阻止
    std::string redirectTo;    // 重定向目标（为空则不重定向）
};

// 全局过滤规则
std::vector<FilterRule> websiteRules;  // 网站过滤规则
std::vector<FilterRule> userRules;     // 用户过滤规则
std::map<std::string, std::string> redirectRules; // 网站引导规则

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

// 新增：过滤和引导相关函数
void LoadFilterRules();
BOOL CheckWebsiteAccess(const char *hostname);
BOOL CheckUserAccess(const char *clientIP);
BOOL GetRedirectTarget(const char *hostname, char *redirectHost, int *redirectPort);
void SendBlockedResponse(SOCKET clientSocket);
void SendRedirectResponse(SOCKET clientSocket, const char *redirectUrl);

// 代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

// 代理参数结构
struct ProxyParam {
    SOCKET clientSocket;
    SOCKET serverSocket;
    char clientIP[32];  // 客户端IP地址
};

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

    if (!InitSocket()) {
        printf("Socket初始化失败\n");
        return -1;
    }

    printf("代理服务器启动成功!\n");
    printf("监听端口: %d\n", ProxyPort);
    printf("缓存目录: %s\n", CACHE_DIR);
    printf("网站过滤规则: %d 条\n", websiteRules.size());
    printf("用户过滤规则: %d 条\n", userRules.size());
    printf("网站引导规则: %d 条\n", redirectRules.size());
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

        lpProxyParam->clientSocket = acceptSocket;
        lpProxyParam->serverSocket = INVALID_SOCKET;
        strcpy_s(lpProxyParam->clientIP, sizeof(lpProxyParam->clientIP), inet_ntoa(clientAddr.sin_addr));

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
        _mkdir(CACHE_DIR);
        printf("[缓存] 创建缓存目录: %s\n", CACHE_DIR);
    }
}

// 加载过滤规则
void LoadFilterRules() {
    FILE *file = NULL;
    char line[512];

    // 加载网站过滤规则（website_filter.txt）
    fopen_s(&file, "website_filter.txt", "r");
    if (file != NULL) {
        printf("[配置] 加载网站过滤规则...\n");
        while (fgets(line, sizeof(line), file)) {
            // 跳过注释和空行
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

            // 去除换行符
            line[strcspn(line, "\r\n")] = 0;

            FilterRule rule;
            char action[16];
            char hostname[256];

            // 解析格式: allow/deny hostname
            if (sscanf_s(line, "%s %s", action, (unsigned)sizeof(action), hostname, (unsigned)sizeof(hostname)) == 2) {
                rule.hostname = hostname;
                rule.isAllowed = (strcmp(action, "allow") == 0);
                websiteRules.push_back(rule);
                printf("[配置]   %s %s\n", action, hostname);
            }
        }
        fclose(file);
    } else {
        printf("[配置] 未找到 website_filter.txt，跳过网站过滤\n");
    }

    // 加载用户过滤规则（user_filter.txt）
    fopen_s(&file, "user_filter.txt", "r");
    if (file != NULL) {
        printf("[配置] 加载用户过滤规则...\n");
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            line[strcspn(line, "\r\n")] = 0;

            FilterRule rule;
            char action[16];
            char ip[32];

            // 解析格式: allow/deny IP
            if (sscanf_s(line, "%s %s", action, (unsigned)sizeof(action), ip, (unsigned)sizeof(ip)) == 2) {
                rule.clientIP = ip;
                rule.isAllowed = (strcmp(action, "allow") == 0);
                userRules.push_back(rule);
                printf("[配置]   %s %s\n", action, ip);
            }
        }
        fclose(file);
    } else {
        printf("[配置] 未找到 user_filter.txt，跳过用户过滤\n");
    }

    // 加载网站引导规则（redirect.txt）
    fopen_s(&file, "redirect.txt", "r");
    if (file != NULL) {
        printf("[配置] 加载网站引导规则...\n");
        while (fgets(line, sizeof(line), file)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            line[strcspn(line, "\r\n")] = 0;

            char source[256];
            char target[256];

            // 解析格式: source_host target_host
            if (sscanf_s(line, "%s %s", source, (unsigned)sizeof(source), target, (unsigned)sizeof(target)) == 2) {
                redirectRules[source] = target;
                printf("[配置]   %s -> %s\n", source, target);
            }
        }
        fclose(file);
    } else {
        printf("[配置] 未找到 redirect.txt，跳过网站引导\n");
    }
}

// 检查网站访问权限
BOOL CheckWebsiteAccess(const char *hostname) {
    // 如果没有规则，默认允许
    if (websiteRules.empty()) return TRUE;

    // 遍历规则查找匹配
    for (size_t i = 0; i < websiteRules.size(); i++) {
        // 支持通配符匹配（简单实现：检查子串）
        if (strstr(hostname, websiteRules[i].hostname.c_str()) != NULL) {
            return websiteRules[i].isAllowed;
        }
    }

    // 默认行为：如果有规则但没匹配，允许访问
    return TRUE;
}

// 检查用户访问权限
BOOL CheckUserAccess(const char *clientIP) {
    // 如果没有规则，默认允许
    if (userRules.empty()) return TRUE;

    // 遍历规则查找匹配
    for (size_t i = 0; i < userRules.size(); i++) {
        if (strcmp(clientIP, userRules[i].clientIP.c_str()) == 0) {
            return userRules[i].isAllowed;
        }
    }

    // 默认行为：如果有规则但没匹配，允许访问
    return TRUE;
}

// 获取重定向目标
BOOL GetRedirectTarget(const char *hostname, char *redirectHost, int *redirectPort) {
    std::string host(hostname);

    if (redirectRules.find(host) != redirectRules.end()) {
        std::string target = redirectRules[host];

        // 解析目标（支持 host:port 格式）
        size_t colonPos = target.find(':');
        if (colonPos != std::string::npos) {
            strcpy_s(redirectHost, 1024, target.substr(0, colonPos).c_str());
            *redirectPort = atoi(target.substr(colonPos + 1).c_str());
        } else {
            strcpy_s(redirectHost, 1024, target.c_str());
            *redirectPort = 80;
        }
        return TRUE;
    }

    return FALSE;
}

// 发送阻止响应
void SendBlockedResponse(SOCKET clientSocket) {
    char response[1024];
    sprintf_s(response, sizeof(response),
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        strlen(BLOCKED_PAGE_HTML),
        BLOCKED_PAGE_HTML);

    send(clientSocket, response, strlen(response), 0);
}

// 函数：初始化套接字
BOOL InitSocket() {
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

    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == ProxyServer) {
        printf("创建套接字失败,错误代码为:%d\n", WSAGetLastError());
        return FALSE;
    }

    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(ProxyPort);
    ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        printf("绑定套接字失败\n");
        closesocket(ProxyServer);
        return FALSE;
    }

    if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
        printf("监听端口%d失败\n", ProxyPort);
        closesocket(ProxyServer);
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
    if (!CheckWebsiteAccess(httpHeader->host)) {
        printf("[网站过滤] 阻止访问网站: %s\n", httpHeader->host);
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

// 解析HTTP请求头部
void ParseHttpHead(char *buffer, HttpHeader *httpHeader) {
    char *p;
    char *ptr;
    const char *delim = "\r\n";

    p = strtok_s(buffer, delim, &ptr);
    if (p == NULL) return;

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

    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        if (strncmp(p, "Host:", 5) == 0) {
            char *host_start = p + 6;
            while (*host_start == ' ') host_start++;

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

    p = strtok_s(tempBuffer, delim, &ptr);
    if (p != NULL) {
        char *statusStart = strchr(p, ' ');
        if (statusStart != NULL) {
            httpResponse->statusCode = atoi(statusStart + 1);
        }
    }

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

// 生成缓存键
void GenerateCacheKey(const char *host, int port, const char *url, char *cacheKey) {
    char fullUrl[2048];
    sprintf_s(fullUrl, sizeof(fullUrl), "%s:%d%s", host, port, url);

    unsigned int hash = 0;
    for (int i = 0; fullUrl[i] != '\0'; i++) {
        hash = hash * 31 + fullUrl[i];
    }

    sprintf_s(cacheKey, 512, "%s\\%u.cache", CACHE_DIR, hash);
}

// 加载缓存
BOOL LoadCache(const char *cacheKey, char **data, int *dataSize, char *lastModified) {
    FILE *file = NULL;
    fopen_s(&file, cacheKey, "rb");
    if (file == NULL) {
        return FALSE;
    }

    char lm[128] = {0};
    fread(lm, 1, 128, file);
    strcpy_s(lastModified, 128, lm);

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 128, SEEK_SET);

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

    char lm[128] = {0};
    strcpy_s(lm, sizeof(lm), lastModified);
    fwrite(lm, 1, 128, file);

    fwrite(data, 1, dataSize, file);

    fclose(file);
    return TRUE;
}

// 修改请求添加If-Modified-Since头
void ModifyRequestWithCache(char *request, int *requestSize, const char *lastModified) {
    char *headerEnd = strstr(request, "\r\n\r\n");
    if (headerEnd == NULL) return;

    if (strstr(request, "If-Modified-Since:") != NULL) {
        return;
    }

    char ifModifiedSince[256];
    sprintf_s(ifModifiedSince, sizeof(ifModifiedSince), "If-Modified-Since: %s\r\n", lastModified);

    int headerLength = headerEnd - request;
    int newSize = headerLength + strlen(ifModifiedSince) + 4;

    char *newRequest = new char[MAXSIZE];
    memcpy(newRequest, request, headerLength + 2);
    strcpy_s(newRequest + headerLength + 2, MAXSIZE - headerLength - 2, ifModifiedSince);
    strcat_s(newRequest, MAXSIZE, "\r\n");

    int bodyLength = *requestSize - (headerEnd - request) - 4;
    if (bodyLength > 0) {
        memcpy(newRequest + newSize, headerEnd + 4, bodyLength);
        newSize += bodyLength;
    }

    memcpy(request, newRequest, newSize);
    *requestSize = newSize;

    delete[] newRequest;
}
