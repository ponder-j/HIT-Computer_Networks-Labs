/**
 * @file http_parser.cpp
 * @brief HTTP协议解析器实现
 * @details 提供HTTP请求和响应的解析功能，支持请求头修改
 */

#include "../include/http_parser.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief 解析HTTP请求头部
 * @param buffer 接收到的HTTP请求原始数据缓冲区
 * @param httpHeader 用于存储解析结果的HTTP头部结构体指针
 * @details 该函数解析HTTP请求的第一行和各个头部字段，提取请求方法、URL、主机名、端口和Cookie信息
 */
void ParseHttpHead(char *buffer, HttpHeader *httpHeader) {
    char *p;        // 当前解析的行指针
    char *ptr;      // strtok_s的上下文指针，用于保存分割状态
    const char *delim = "\r\n";  // HTTP头部行分隔符

    // 第一步：解析请求行（Request Line）
    // 格式：METHOD URL HTTP/VERSION
    p = strtok_s(buffer, delim, &ptr);
    if (p == NULL) return;  // 如果没有内容则直接返回

    // 第二步：判断HTTP请求方法并提取URL
    if (strncmp(p, "GET", 3) == 0) {
        // 处理GET请求：GET /path HTTP/1.1
        memcpy(httpHeader->method, "GET", 3);
        char *url_start = p + 4;  // 跳过"GET "（4个字符）
        char *url_end = strstr(url_start, " HTTP");  // 找到URL结束位置
        if (url_end != NULL) {
            int url_len = url_end - url_start;
            if (url_len < 1024) {  // 防止缓冲区溢出
                memcpy(httpHeader->url, url_start, url_len);
            }
        }
    }
    else if (strncmp(p, "POST", 4) == 0) {
        // 处理POST请求：POST /path HTTP/1.1
        memcpy(httpHeader->method, "POST", 4);
        char *url_start = p + 5;  // 跳过"POST "（5个字符）
        char *url_end = strstr(url_start, " HTTP");  // 找到URL结束位置
        if (url_end != NULL) {
            int url_len = url_end - url_start;
            if (url_len < 1024) {  // 防止缓冲区溢出
                memcpy(httpHeader->url, url_start, url_len);
            }
        }
    }
    else if (strncmp(p, "CONNECT", 7) == 0) {
        // 处理CONNECT请求（用于HTTPS隧道）：CONNECT host:port HTTP/1.1
        memcpy(httpHeader->method, "CONNECT", 7);
    }

    // 第三步：逐行解析HTTP请求头部字段
    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        // 解析Host字段：Host: www.example.com:8080
        if (strncmp(p, "Host:", 5) == 0) {
            char *host_start = p + 6;  // 跳过"Host: "
            while (*host_start == ' ') host_start++;  // 跳过可能的空格

            // 查找是否包含端口号（通过冒号分隔）
            char *colon = strchr(host_start, ':');
            if (colon != NULL) {
                // 存在端口号：分别提取主机名和端口
                int host_len = colon - host_start;
                if (host_len < 1024) {  // 防止缓冲区溢出
                    memcpy(httpHeader->host, host_start, host_len);
                    httpHeader->host[host_len] = '\0';  // 添加字符串结束符
                }
                httpHeader->port = atoi(colon + 1);  // 提取端口号
            } else {
                // 不存在端口号：使用默认HTTP端口80
                strcpy_s(httpHeader->host, sizeof(httpHeader->host), host_start);
                httpHeader->port = 80;
            }
        }
        // 解析Cookie字段：Cookie: session_id=abc123; user=test
        else if (strncmp(p, "Cookie:", 7) == 0) {
            char *cookie_start = p + 8;  // 跳过"Cookie: "
            while (*cookie_start == ' ') cookie_start++;  // 跳过可能的空格
            strcpy_s(httpHeader->cookie, sizeof(httpHeader->cookie), cookie_start);
        }
        p = strtok_s(NULL, delim, &ptr);  // 继续解析下一行
    }
}

/**
 * @brief 解析HTTP响应头部
 * @param buffer 接收到的HTTP响应原始数据缓冲区
 * @param bufferSize 缓冲区大小
 * @param httpResponse 用于存储解析结果的HTTP响应结构体指针
 * @details 该函数解析HTTP响应的状态行和各个头部字段，提取状态码、Last-Modified、Content-Type和Content-Length信息
 */
void ParseHttpResponse(char *buffer, int bufferSize, HttpResponse *httpResponse) {
    // 第一步：查找HTTP响应头部结束位置（双换行符标记）
    char *headerEnd = strstr(buffer, "\r\n\r\n");
    if (headerEnd == NULL) return;  // 如果没有找到完整头部则返回

    // 第二步：创建临时缓冲区存储头部内容（不包括响应体）
    char *tempBuffer = new char[headerEnd - buffer + 1];
    memcpy(tempBuffer, buffer, headerEnd - buffer);
    tempBuffer[headerEnd - buffer] = '\0';  // 添加字符串结束符

    char *p;        // 当前解析的行指针
    char *ptr;      // strtok_s的上下文指针
    const char *delim = "\r\n";  // HTTP头部行分隔符

    // 第三步：解析状态行（Status Line）
    // 格式：HTTP/VERSION STATUS_CODE REASON_PHRASE
    // 例如：HTTP/1.1 200 OK
    p = strtok_s(tempBuffer, delim, &ptr);
    if (p != NULL) {
        char *statusStart = strchr(p, ' ');  // 找到第一个空格
        if (statusStart != NULL) {
            httpResponse->statusCode = atoi(statusStart + 1);  // 提取状态码（200, 304, 404等）
        }
    }

    // 第四步：逐行解析HTTP响应头部字段
    p = strtok_s(NULL, delim, &ptr);
    while (p) {
        // 解析Last-Modified字段：用于缓存验证
        // 格式：Last-Modified: Wed, 21 Oct 2025 07:28:00 GMT
        if (strncmp(p, "Last-Modified:", 14) == 0) {
            char *value = p + 15;  // 跳过"Last-Modified: "
            while (*value == ' ') value++;  // 跳过可能的空格
            strcpy_s(httpResponse->lastModified, sizeof(httpResponse->lastModified), value);
            httpResponse->hasLastModified = true;  // 标记存在Last-Modified字段
        }
        // 解析Content-Type字段：指示响应体的MIME类型
        // 格式：Content-Type: text/html; charset=utf-8
        else if (strncmp(p, "Content-Type:", 13) == 0) {
            char *value = p + 14;  // 跳过"Content-Type: "
            while (*value == ' ') value++;  // 跳过可能的空格
            strcpy_s(httpResponse->contentType, sizeof(httpResponse->contentType), value);
        }
        // 解析Content-Length字段：指示响应体的字节长度
        // 格式：Content-Length: 1234
        else if (strncmp(p, "Content-Length:", 15) == 0) {
            char *value = p + 16;  // 跳过"Content-Length: "
            while (*value == ' ') value++;  // 跳过可能的空格
            httpResponse->contentLength = atoi(value);  // 转换为整数
        }
        p = strtok_s(NULL, delim, &ptr);  // 继续解析下一行
    }

    // 第五步：释放临时缓冲区内存
    delete[] tempBuffer;
}

/**
 * @brief 修改HTTP请求，添加If-Modified-Since头部用于缓存验证
 * @param request HTTP请求数据缓冲区（将被修改）
 * @param requestSize HTTP请求数据大小的指针（将被更新）
 * @param lastModified 上次修改时间字符串（从缓存中获取）
 * @details 该函数在原始HTTP请求中插入If-Modified-Since头部，
 *          使服务器可以判断资源是否被修改，如果未修改则返回304状态码
 */
void ModifyRequestWithCache(char *request, int *requestSize, const char *lastModified) {
    // 第一步：查找HTTP请求头部结束位置
    char *headerEnd = strstr(request, "\r\n\r\n");
    if (headerEnd == NULL) return;  // 如果没有找到完整头部则返回

    // 第二步：检查请求中是否已经包含If-Modified-Since头部
    // 如果已存在，则不需要重复添加
    if (strstr(request, "If-Modified-Since:") != NULL) {
        return;
    }

    // 第三步：构造If-Modified-Since头部字段
    // 格式：If-Modified-Since: Wed, 21 Oct 2025 07:28:00 GMT
    char ifModifiedSince[256];
    sprintf_s(ifModifiedSince, sizeof(ifModifiedSince), "If-Modified-Since: %s\r\n", lastModified);

    // 第四步：计算插入新头部后的请求大小
    int headerLength = headerEnd - request;  // 原始头部长度（不含\r\n\r\n）
    int newSize = headerLength + strlen(ifModifiedSince) + 4;  // 新请求大小（+2是第一个\r\n，+2是结尾\r\n）

    // 第五步：创建新的请求缓冲区并重组数据
    char *newRequest = new char[MAXSIZE];
    
    // 复制原始头部（包括第一个\r\n）
    memcpy(newRequest, request, headerLength + 2);
    
    // 插入If-Modified-Since头部
    strcpy_s(newRequest + headerLength + 2, MAXSIZE - headerLength - 2, ifModifiedSince);
    
    // 添加头部结束标记\r\n
    strcat_s(newRequest, MAXSIZE, "\r\n");

    // 第六步：如果存在请求体（POST请求等），则复制请求体数据
    int bodyLength = *requestSize - (headerEnd - request) - 4;
    if (bodyLength > 0) {
        memcpy(newRequest + newSize, headerEnd + 4, bodyLength);
        newSize += bodyLength;
    }

    // 第七步：用修改后的请求替换原始请求
    memcpy(request, newRequest, newSize);
    *requestSize = newSize;  // 更新请求大小

    // 第八步：释放临时缓冲区内存
    delete[] newRequest;
}
