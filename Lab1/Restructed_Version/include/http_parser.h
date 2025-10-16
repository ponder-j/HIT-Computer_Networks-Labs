#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "http_structs.h"

// HTTP头部解析
void ParseHttpHead(char *buffer, HttpHeader *httpHeader);

// HTTP响应头部解析
void ParseHttpResponse(char *buffer, int bufferSize, HttpResponse *httpResponse);

// 修改请求添加If-Modified-Since头
void ModifyRequestWithCache(char *request, int *requestSize, const char *lastModified);

#endif // HTTP_PARSER_H
