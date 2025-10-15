# HTTP 代理服务器 - 带缓存版本

## 功能特性

### 基础功能
- 支持 HTTP GET/POST 请求代理
- 支持任意端口（从 Host 头部解析）
- 多线程处理并发连接
- 自动忽略 HTTPS CONNECT 请求

### 缓存功能
1. **智能缓存**：仅对 GET 请求启用缓存
2. **条件请求**：使用 `If-Modified-Since` 头向原服务器验证缓存
3. **304 处理**：服务器返回 304 Not Modified 时使用缓存
4. **Last-Modified**：提取并保存响应的 Last-Modified 时间
5. **文件缓存**：缓存保存在 `./cache` 目录

## 编译方法

### 使用批处理脚本
```batch
compile_cache.bat
```

### 手动编译
```batch
g++ -o ProxyServerWithCache.exe ProxyServerWithCache.cpp -lws2_32 -static
```

## 运行方法

```batch
ProxyServerWithCache.exe
```

代理服务器将在 **10240** 端口监听。

## 浏览器配置

### Firefox
1. 打开 设置 → 网络设置 → 设置
2. 选择"手动代理配置"
3. HTTP 代理: `127.0.0.1`，端口: `10240`
4. 勾选"也将此代理用于 HTTPS"（可选）

### Chrome/Edge
1. 打开 设置 → 系统 → 打开代理设置
2. 手动设置代理
3. HTTP 代理: `127.0.0.1:10240`

## 缓存工作流程

### 首次请求（无缓存）
```
客户端 → 代理服务器 → 原服务器
                    ← 200 OK (包含 Last-Modified)
       ← 转发响应
[保存缓存 + Last-Modified]
```

### 再次请求（有缓存）
```
客户端 → 代理服务器 → 原服务器 (添加 If-Modified-Since)
                    ← 304 Not Modified
       ← 返回缓存数据（直接从原响应转发）
[缓存仍然有效]
```

或者：
```
客户端 → 代理服务器 → 原服务器 (添加 If-Modified-Since)
                    ← 200 OK (内容已更新)
       ← 转发新响应
[更新缓存]
```

## 核心实现

### 1. 缓存数据结构
```cpp
struct CacheItem {
    char url[2048];              // 完整URL
    char lastModified[128];      // Last-Modified时间
    char filePath[512];          // 缓存文件路径
    int dataSize;                // 数据大小
    time_t cacheTime;            // 缓存时间
};
```

### 2. 缓存键生成
使用 `host:port/url` 的哈希值作为缓存文件名：
```cpp
void GenerateCacheKey(const char *host, int port, const char *url, char *cacheKey);
```

### 3. 解析响应头
提取 `Last-Modified`、`Content-Type`、状态码等信息：
```cpp
void ParseHttpResponse(char *buffer, int bufferSize, HttpResponse *httpResponse);
```

### 4. 条件请求
在请求中添加 `If-Modified-Since` 头：
```cpp
void ModifyRequestWithCache(char *request, int *requestSize, const char *lastModified);
```

### 5. 缓存存储格式
```
[Last-Modified (128 bytes)][HTTP Response Data]
```

## 示例输出

```
========================================
   HTTP 代理服务器 v2.0 (带缓存)
========================================
代理服务器正在启动...
[缓存] 创建缓存目录: .\cache
初始化Socket...
代理服务器启动成功!
监听端口: 10240
缓存目录: .\cache
等待客户端连接...
========================================

[新连接] 客户端: 127.0.0.1:52341
[接收] 收到 423 字节的HTTP请求
[解析] 方法: GET, 主机: www.example.com:80, URL: /index.html
[缓存] 未找到缓存
[连接] 成功连接到目标服务器: www.example.com:80
[转发] 已转发请求到目标服务器 (423 字节)
[响应] 状态码: 200
[响应] Last-Modified: Mon, 13 Jan 2025 12:00:00 GMT
[完成] 已转发响应到客户端 (总计 1534 字节)
[缓存] 已保存缓存，大小: 1534 字节

[新连接] 客户端: 127.0.0.1:52342
[接收] 收到 423 字节的HTTP请求
[解析] 方法: GET, 主机: www.example.com:80, URL: /index.html
[缓存] 找到缓存，大小: 1534 字节, Last-Modified: Mon, 13 Jan 2025 12:00:00 GMT
[缓存] 添加 If-Modified-Since: Mon, 13 Jan 2025 12:00:00 GMT
[连接] 成功连接到目标服务器: www.example.com:80
[转发] 已转发请求到目标服务器 (478 字节)
[响应] 状态码: 304
[完成] 已转发响应到客户端 (总计 145 字节)
[缓存] 服务器返回304 Not Modified，缓存仍然有效
```

## 注意事项

1. **仅支持 HTTP**：不支持 HTTPS（会自动忽略 CONNECT 请求）
2. **GET 请求缓存**：只有 GET 请求会被缓存，POST 请求直接转发
3. **Last-Modified 必需**：只缓存包含 Last-Modified 头的响应
4. **缓存目录**：首次运行会自动创建 `./cache` 目录
5. **缓存大小限制**：单个响应最大 655MB（MAXSIZE * 10）

## 测试建议

1. 访问包含 Last-Modified 头的静态网站（如 Apache/Nginx 服务器）
2. 第一次访问查看是否保存缓存
3. 刷新页面查看是否发送 If-Modified-Since
4. 检查 `./cache` 目录中的缓存文件

## 与基础版本的对比

| 功能 | ProxyServer.cpp | ProxyServerWithCache.cpp |
|------|----------------|--------------------------|
| HTTP 代理 | ✅ | ✅ |
| 多端口支持 | ✅ | ✅ |
| 缓存功能 | ❌ | ✅ |
| If-Modified-Since | ❌ | ✅ |
| 304 处理 | ❌ | ✅ |
| Last-Modified 解析 | ❌ | ✅ |

## 文件说明

- `ProxyServerWithCache.cpp` - 带缓存的代理服务器源代码
- `compile_cache.bat` - 编译脚本
- `./cache/` - 缓存文件存储目录（自动创建）
