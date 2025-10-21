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

    // 多项式滚动哈希函数
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
    fread(lm, 1, 128, file); // 读取Last-Modified时间戳
    strcpy_s(lastModified, 128, lm); // 保存到输出参数

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file); // 获取文件大小
    fseek(file, 128, SEEK_SET); // 跳过时间戳部分

    int contentSize = fileSize - 128; // 计算内容大小
    *data = new char[contentSize]; // 分配内存
    *dataSize = fread(*data, 1, contentSize, file); // 读取缓存内容到 data，返回实际读取大小 dataSize

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
