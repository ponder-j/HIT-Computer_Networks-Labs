#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "http_structs.h"

// 初始化缓存目录
void InitCacheDirectory();

// 生成缓存键
void GenerateCacheKey(const char *host, int port, const char *url, char *cacheKey);

// 加载缓存
BOOL LoadCache(const char *cacheKey, char **data, int *dataSize, char *lastModified);

// 保存缓存
BOOL SaveCache(const char *cacheKey, const char *data, int dataSize, const char *lastModified);

#endif // CACHE_MANAGER_H
