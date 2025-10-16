#include "../include/http_parser.h"
#include <stdio.h>
#include <string.h>

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
