#include "../include/cache_manager.h"
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <sys/stat.h>

// 初始化缓存目录
void InitCacheDirectory() {
    struct stat info;
    if (stat(CACHE_DIR, &info) != 0) {
        _mkdir(CACHE_DIR);
        printf("[缓存] 创建缓存目录: %s\n", CACHE_DIR);
    }
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
