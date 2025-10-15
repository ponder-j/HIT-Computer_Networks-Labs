# HTTP 代理服务器 v3.0 - 高级版

## 🎯 功能概述

ProxyServerAdvanced 是一个功能完整的 HTTP 代理服务器，支持：

1. **HTTP 缓存**：使用 If-Modified-Since/Last-Modified 机制缓存网页
2. **网站过滤**：允许/阻止访问特定网站
3. **用户过滤**：支持/阻止特定用户（基于IP）访问外部网站
4. **网站引导**：将用户对某网站的访问重定向到模拟网站（钓鱼模拟）

## 📦 文件说明

```
Lab1/
├── ProxyServerAdvanced.cpp    # 主程序源代码
├── ProxyServerAdvanced.exe    # 编译后的可执行文件
├── compile_advanced.bat       # 编译脚本
├── website_filter.txt         # 网站过滤规则
├── user_filter.txt            # 用户过滤规则
├── redirect.txt               # 网站引导（重定向）规则
├── cache/                     # 缓存目录（自动创建）
└── ADVANCED_README.md         # 本文档
```

## 🚀 快速开始

### 1. 编译程序

**Windows 环境（MinGW/GCC）：**
```bash
# 方法1：使用批处理脚本
compile_advanced.bat

# 方法2：直接编译
g++ -o ProxyServerAdvanced.exe ProxyServerAdvanced.cpp -lws2_32 -static -lstdc++
```

### 2. 配置过滤规则

编辑配置文件以设置过滤和引导规则：

**website_filter.txt** - 网站过滤
```
deny facebook.com      # 阻止访问 Facebook
deny twitter.com       # 阻止访问 Twitter
allow github.com       # 允许访问 GitHub
```

**user_filter.txt** - 用户过滤
```
deny 192.168.1.100     # 阻止此IP访问代理
allow 192.168.1.50     # 允许此IP访问代理
```

**redirect.txt** - 网站引导（钓鱼）
```
baidu.com localhost:80           # 将百度重定向到本地
google.com fake-google.com       # 将Google重定向到模拟网站
```

### 3. 启动代理服务器

```bash
ProxyServerAdvanced.exe
```

启动后会显示：
```
========================================
   HTTP 代理服务器 v3.0 (高级版)
   支持: 缓存 + 过滤 + 引导
========================================
代理服务器正在启动...
[缓存] 创建缓存目录: .\cache
[配置] 加载网站过滤规则...
[配置]   deny facebook.com
[配置]   deny twitter.com
[配置] 加载用户过滤规则...
[配置]   deny 192.168.1.100
[配置] 加载网站引导规则...
[配置]   baidu.com -> localhost:80
代理服务器启动成功!
监听端口: 10240
缓存目录: .\cache
网站过滤规则: 2 条
用户过滤规则: 1 条
网站引导规则: 1 条
等待客户端连接...
========================================
```

### 4. 配置浏览器代理

**Firefox：**
- 设置 → 网络设置 → 手动代理配置
- HTTP 代理: `127.0.0.1`, 端口: `10240`

**Chrome/Edge：**
- 设置 → 系统 → 代理设置
- HTTP 代理: `127.0.0.1:10240`

### 5. 测试功能

访问配置的网站，观察代理服务器日志输出。

## 📋 功能详解

### 1. HTTP 缓存

**工作原理：**
- 首次请求：代理转发请求，保存响应和 Last-Modified 时间
- 后续请求：代理添加 If-Modified-Since 头
- 服务器返回 304：使用缓存
- 服务器返回 200：更新缓存

**日志示例：**
```
[缓存] 未找到缓存
[响应] 状态码: 200
[响应] Last-Modified: Mon, 15 Jan 2025 10:00:00 GMT
[缓存] 已保存缓存，大小: 12345 字节

[缓存] 找到缓存，大小: 12345 字节
[缓存] 添加 If-Modified-Since: Mon, 15 Jan 2025 10:00:00 GMT
[响应] 状态码: 304
[缓存] 服务器返回304 Not Modified，缓存仍然有效
```

### 2. 网站过滤

**配置文件：** `website_filter.txt`

**格式：**
```
allow/deny hostname
```

**匹配规则：**
- 支持子串匹配：`baidu.com` 匹配 `www.baidu.com`, `tieba.baidu.com` 等
- 按顺序匹配，第一个匹配的规则生效
- 无匹配规则时默认允许访问

**示例配置：**
```
# 阻止社交媒体
deny facebook.com
deny twitter.com
deny instagram.com

# 阻止视频网站
deny youtube.com
deny youku.com

# 阻止特定域名
deny gambling-site.com
deny malicious-site.com

# 明确允许（可选）
allow github.com
allow stackoverflow.com
```

**日志示例：**
```
[解析] 方法: GET, 主机: facebook.com:80, URL: /
[网站过滤] 阻止访问网站: facebook.com
```

**被阻止时的响应：**
浏览器会显示：
```
403 Forbidden
该网站已被管理员阻止访问。
```

### 3. 用户过滤

**配置文件：** `user_filter.txt`

**格式：**
```
allow/deny IP地址
```

**匹配规则：**
- 必须精确匹配IP地址
- 按顺序匹配，第一个匹配的规则生效
- 无匹配规则时默认允许访问

**示例配置：**
```
# 阻止特定用户
deny 192.168.1.100
deny 192.168.1.101
deny 10.0.0.50

# 仅允许特定用户（配合deny *实现白名单）
allow 192.168.1.1
allow 192.168.1.2
```

**日志示例：**
```
[新连接] 客户端: 192.168.1.100:54321
[用户过滤] 阻止用户访问: 192.168.1.100
```

**应用场景：**
- 限制办公网络中特定电脑的上网权限
- 创建白名单，只允许授权IP访问代理
- 临时封禁某个用户

### 4. 网站引导（钓鱼模拟）

**配置文件：** `redirect.txt`

**格式：**
```
源网站 目标网站[:端口]
```

**匹配规则：**
- 必须精确匹配主机名
- 可以指定目标端口，不指定则默认80
- 支持重定向到本地服务器

**示例配置：**
```
# 重定向到本地模拟网站
baidu.com localhost:80
google.com localhost:8080

# 重定向到其他域名
facebook.com fake-facebook.com
twitter.com fake-twitter.com:8080

# 钓鱼测试
bank.com phishing-site.com
```

**日志示例：**
```
[解析] 方法: GET, 主机: baidu.com:80, URL: /
[网站引导] baidu.com -> localhost:80
[连接] 成功连接到目标服务器: localhost:80
```

**工作原理：**
1. 客户端请求访问 `baidu.com`
2. 代理检测到重定向规则
3. 代理修改目标主机为 `localhost`
4. 代理连接到 `localhost:80` 而非 `baidu.com`
5. 返回本地服务器的内容给客户端
6. 客户端浏览器地址栏仍显示 `baidu.com`（透明代理）

**应用场景：**
- 网络钓鱼攻击演示（教育目的）
- 网站模拟和测试
- 本地开发环境模拟生产环境
- 安全意识培训

## 🧪 测试示例

### 测试1：网站过滤

**配置：**
```bash
# website_filter.txt
deny baidu.com
```

**测试步骤：**
1. 启动代理服务器
2. 在浏览器中访问 `http://www.baidu.com`
3. 观察结果

**预期结果：**
- 浏览器显示 "403 Forbidden" 页面
- 代理日志显示：`[网站过滤] 阻止访问网站: www.baidu.com`

### 测试2：用户过滤

**配置：**
```bash
# user_filter.txt
deny 127.0.0.1
```

**测试步骤：**
1. 启动代理服务器
2. 从本机（127.0.0.1）通过代理访问任何网站
3. 观察结果

**预期结果：**
- 所有请求被阻止
- 代理日志显示：`[用户过滤] 阻止用户访问: 127.0.0.1`

### 测试3：网站引导（钓鱼）

**前提条件：**
- 在 `localhost:80` 运行一个测试网站（使用 Lab1/test_server）

**配置：**
```bash
# redirect.txt
baidu.com localhost
```

**测试步骤：**
1. 启动本地测试服务器（端口80）
2. 启动代理服务器
3. 在浏览器中访问 `http://www.baidu.com`
4. 观察结果

**预期结果：**
- 浏览器地址栏显示 `www.baidu.com`
- 页面内容为本地测试服务器的内容
- 代理日志显示：`[网站引导] www.baidu.com -> localhost:80`

### 测试4：组合测试

**配置：**
```bash
# website_filter.txt
deny qq.com

# user_filter.txt
allow 127.0.0.1

# redirect.txt
baidu.com localhost
```

**测试：**
1. 访问 `baidu.com` → 重定向到本地 ✅
2. 访问 `qq.com` → 被阻止 ❌
3. 访问 `github.com` → 正常访问 ✅

## 📊 日志解读

### 完整请求流程日志

```
[新连接] 客户端: 127.0.0.1:54321
[接收] 收到 456 字节的HTTP请求
[解析] 方法: GET, 主机: www.baidu.com:80, URL: /
[网站引导] www.baidu.com -> localhost:80
[缓存] 找到缓存，大小: 12345 字节, Last-Modified: Mon, 15 Jan 2025 10:00:00 GMT
[缓存] 添加 If-Modified-Since: Mon, 15 Jan 2025 10:00:00 GMT
[连接] 成功连接到目标服务器: localhost:80
[转发] 已转发请求到目标服务器 (456 字节)
[响应] 状态码: 304
[响应] Last-Modified: Mon, 15 Jan 2025 10:00:00 GMT
[完成] 已转发响应到客户端 (总计 123 字节)
[缓存] 服务器返回304 Not Modified，缓存仍然有效
```

### 关键日志标识

| 标识 | 含义 |
|------|------|
| `[新连接]` | 新的客户端连接 |
| `[接收]` | 接收到HTTP请求 |
| `[解析]` | 解析HTTP头部信息 |
| `[用户过滤]` | 用户过滤检查结果 |
| `[网站过滤]` | 网站过滤检查结果 |
| `[网站引导]` | 网站重定向操作 |
| `[缓存]` | 缓存相关操作 |
| `[连接]` | 连接目标服务器 |
| `[转发]` | 转发请求/响应 |
| `[响应]` | 服务器响应信息 |
| `[完成]` | 请求处理完成 |
| `[错误]` | 错误信息 |

## 🔧 高级配置

### 策略组合示例

#### 场景1：企业网络管理

**目标：**
- 允许所有员工访问工作相关网站
- 阻止社交媒体和娱乐网站
- 特定IP（如老板办公室）不受限制

**配置：**
```bash
# website_filter.txt
deny facebook.com
deny twitter.com
deny youtube.com
deny netflix.com
allow github.com
allow stackoverflow.com

# user_filter.txt
allow 192.168.1.1    # 老板办公室，不受限制
# 其他IP默认受网站过滤影响
```

#### 场景2：网络安全培训

**目标：**
- 演示钓鱼攻击
- 将银行网站重定向到模拟钓鱼网站

**配置：**
```bash
# redirect.txt
bank-of-america.com localhost:8080
chase.com localhost:8080
wells-fargo.com localhost:8080

# 在localhost:8080运行模拟钓鱼网站
# 用于安全意识培训
```

#### 场景3：家长控制

**目标：**
- 阻止儿童访问不适当内容
- 限制特定设备的访问时间

**配置：**
```bash
# website_filter.txt
deny adult-site.com
deny gambling-site.com
deny violent-game.com

# user_filter.txt
deny 192.168.1.100    # 孩子的电脑
allow 192.168.1.1     # 家长的电脑
```

## ❓ 常见问题

### Q1: 配置文件修改后需要重启代理吗？

**A:** 是的。代理服务器在启动时加载配置文件，修改后需要重启才能生效。

### Q2: 网站过滤规则的优先级是什么？

**A:** 按照配置文件中的顺序，从上到下匹配。第一个匹配的规则生效，后续规则不再检查。

### Q3: 如何实现"仅允许特定网站"（白名单模式）？

**A:** 在 `website_filter.txt` 中，先 deny 所有，再 allow 特定网站：
```
deny *            # 阻止所有（需要代码支持通配符）
allow github.com  # 允许特定网站
allow google.com
```
注：当前版本不支持通配符，需要修改代码实现。

### Q4: 网站引导会改变浏览器地址栏吗？

**A:** 不会。这是透明代理，浏览器地址栏显示的仍是原始URL，但实际内容来自重定向目标。

### Q5: 如何调试配置不生效的问题？

**A:**
1. 检查配置文件格式是否正确（注意空格）
2. 查看启动日志，确认规则已加载
3. 观察请求日志，确认匹配逻辑
4. 确保没有多余的空格或特殊字符

### Q6: 支持HTTPS吗？

**A:** 不支持。当前版本会自动忽略 CONNECT 请求（HTTPS）。要支持HTTPS需要实现SSL/TLS隧道。

### Q7: 如何清除缓存？

**A:** 删除 `cache` 目录中的所有 `.cache` 文件：
```bash
del /Q cache\*.cache    # Windows
rm -rf cache/*.cache    # Linux
```

## 🛡️ 安全注意事项

### 1. 网站引导（钓鱼）功能

**⚠️ 警告：**
- 此功能仅供**教育和研究目的**
- 不得用于非法活动或恶意攻击
- 使用前请确保获得授权
- 用于安全意识培训时，应明确告知用户

**合法使用场景：**
- 企业内部安全培训
- 渗透测试（已授权）
- 网络安全课程演示
- 个人学习和研究

### 2. 用户过滤

- 妥善保管 `user_filter.txt`，避免未授权修改
- 定期审查用户访问权限
- 记录所有过滤日志（可扩展）

### 3. 网站过滤

- 阻止列表应定期更新
- 避免过度过滤影响正常工作
- 提供申诉机制

## 📚 代码扩展建议

### 1. 添加日志文件

当前版本仅输出到控制台，建议添加文件日志：

```cpp
FILE *logFile;
fopen_s(&logFile, "proxy.log", "a");
fprintf(logFile, "[%s] %s\n", timestamp, message);
fclose(logFile);
```

### 2. 支持通配符匹配

修改 `CheckWebsiteAccess` 函数，支持 `*` 通配符：

```cpp
BOOL WildcardMatch(const char *pattern, const char *string) {
    // 实现通配符匹配算法
}
```

### 3. 动态重载配置

添加配置文件监视，自动重载：

```cpp
void WatchConfigFiles() {
    // 使用 FileSystemWatcher 或定时检查
}
```

### 4. 添加统计功能

记录每个网站的访问次数、流量等：

```cpp
struct Statistics {
    std::map<std::string, int> visitCount;
    std::map<std::string, long long> trafficSize;
};
```

### 5. Web管理界面

创建简单的Web界面管理配置：

```cpp
// 启动HTTP管理服务器
void StartAdminServer(int port) {
    // 实现Web界面
}
```

## 📝 版本历史

- **v3.0** (2025-01-15)
  - 新增网站过滤功能
  - 新增用户过滤功能
  - 新增网站引导（钓鱼）功能
  - 保留 v2.0 的缓存功能

- **v2.0** (2025-01-15)
  - 新增HTTP缓存功能
  - 支持 If-Modified-Since 条件请求
  - 支持多端口

- **v1.0** (2025-01-14)
  - 基础HTTP代理功能
  - 支持GET/POST请求转发

## 🤝 贡献

欢迎提交Issue和Pull Request！

## 📄 许可证

本项目仅供学习和研究使用。

---

**开发环境：**
- Windows 10/11
- MinGW GCC 编译器
- Winsock2 网络库

**测试环境：**
- Windows 10
- Firefox / Chrome 浏览器
- Alpine Nginx + PHP 7.3 测试服务器

---

祝使用愉快！🎉
