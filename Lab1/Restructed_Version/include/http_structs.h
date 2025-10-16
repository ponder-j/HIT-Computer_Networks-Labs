#ifndef HTTP_STRUCTS_H
#define HTTP_STRUCTS_H

#include <Windows.h>
#include <string>

// 常量定义
#define MAXSIZE 65507
#define HTTP_PORT 80
#define CACHE_DIR ".\\cache"
#define BLOCKED_PAGE_HTML "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Access Blocked</title></head><body style='font-family: Arial, sans-serif; text-align: center; padding: 50px;'><h1 style='color: #d32f2f;'>403 Forbidden</h1><p style='font-size: 18px;'>This website has been blocked by the proxy administrator.</p><hr><p style='color: #666; font-size: 14px;'>Proxy Server v3.0</p></body></html>"

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
    std::string urlPath;       // URL路径（可选，用于细粒度过滤）
    bool isAllowed;            // true=允许，false=阻止
    std::string redirectTo;    // 重定向目标（为空则不重定向）
};

// 代理参数结构
struct ProxyParam {
    SOCKET clientSocket;
    SOCKET serverSocket;
    char clientIP[32];  // 客户端IP地址
};

#endif // HTTP_STRUCTS_H
