# 高级代理服务器测试指南

## 📋 测试环境准备

### 1. 编译代理服务器

```bash
cd D:\Grade3_Courses\Computer_Networks_Labs\Lab1
compile_advanced.bat
```

### 2. 准备测试服务器

确保你有一个本地HTTP测试服务器用于重定向测试：

**选项1：使用Lab1的测试服务器**
```bash
cd test_server
# 启动你的 Alpine Nginx+PHP 容器
# 确保在 localhost:80 上运行
```

**选项2：使用Python简易服务器**
```bash
cd test_server/www
python -m http.server 80
```

### 3. 准备测试页面（可选）

创建一个简单的模拟钓鱼页面 `phishing.html`：

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>这是一个模拟页面</title>
    <style>
        body {
            font-family: Arial;
            text-align: center;
            padding-top: 100px;
            background: #ffe6e6;
        }
        .warning {
            color: red;
            font-size: 2em;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="warning">⚠️ 网站引导测试成功！</div>
    <p>你以为访问的是：<strong id="original"></strong></p>
    <p>实际访问的是：<strong>localhost</strong></p>
    <p>这就是钓鱼攻击的原理！</p>

    <script>
        // 显示浏览器地址栏中的主机名
        document.getElementById('original').textContent = window.location.hostname;
    </script>
</body>
</html>
```

将此文件放到测试服务器的根目录。

## 🧪 测试用例

### 测试1：基础功能验证

**目标：** 确保代理服务器能正常启动和转发请求

**步骤：**
1. 不配置任何过滤规则（注释掉所有规则）
2. 启动代理服务器
3. 配置浏览器代理为 `127.0.0.1:10240`
4. 访问 `http://www.baidu.com`

**预期结果：**
- ✅ 代理服务器正常启动
- ✅ 显示：网站过滤规则: 0 条
- ✅ 百度页面正常加载
- ✅ 控制台显示请求日志

**日志示例：**
```
[新连接] 客户端: 127.0.0.1:xxxxx
[接收] 收到 xxx 字节的HTTP请求
[解析] 方法: GET, 主机: www.baidu.com:80, URL: /
[连接] 成功连接到目标服务器: www.baidu.com:80
[转发] 已转发请求到目标服务器 (xxx 字节)
[响应] 状态码: 200
[完成] 已转发响应到客户端 (总计 xxx 字节)
```

---

### 测试2：网站过滤 - 阻止访问

**目标：** 验证网站黑名单功能

**配置 `website_filter.txt`：**
```
deny baidu.com
```

**步骤：**
1. 启动代理服务器
2. 访问 `http://www.baidu.com`
3. 访问 `http://www.qq.com`（未被阻止）

**预期结果：**
- ✅ 启动时显示：网站过滤规则: 1 条
- ✅ 访问百度：浏览器显示 "403 Forbidden"
- ✅ 控制台显示：`[网站过滤] 阻止访问网站: www.baidu.com`
- ✅ 访问QQ：正常加载

**浏览器显示：**
```
403 Forbidden
该网站已被管理员阻止访问。
```

**日志示例：**
```
[配置] 加载网站过滤规则...
[配置]   deny baidu.com
网站过滤规则: 1 条

[新连接] 客户端: 127.0.0.1:xxxxx
[接收] 收到 xxx 字节的HTTP请求
[解析] 方法: GET, 主机: www.baidu.com:80, URL: /
[网站过滤] 阻止访问网站: www.baidu.com
```

---

### 测试3：网站过滤 - 多条规则

**目标：** 验证多条过滤规则

**配置 `website_filter.txt`：**
```
deny baidu.com
deny qq.com
deny 163.com
allow github.com
```

**步骤：**
1. 启动代理服务器
2. 依次访问：
   - `http://www.baidu.com` → 应被阻止
   - `http://www.qq.com` → 应被阻止
   - `http://www.163.com` → 应被阻止
   - `http://www.github.com` → 应正常访问
   - `http://www.google.com` → 应正常访问（无规则）

**预期结果：**
- ✅ 启动时显示：网站过滤规则: 4 条
- ✅ 百度、QQ、163 被阻止
- ✅ GitHub、Google 正常访问

---

### 测试4：用户过滤 - 阻止特定IP

**目标：** 验证用户黑名单功能

**配置 `user_filter.txt`：**
```
deny 127.0.0.1
```

**步骤：**
1. 启动代理服务器
2. 从本机（127.0.0.1）访问任何网站

**预期结果：**
- ✅ 启动时显示：用户过滤规则: 1 条
- ✅ 所有访问都被阻止
- ✅ 浏览器显示 "403 Forbidden"
- ✅ 控制台显示：`[用户过滤] 阻止用户访问: 127.0.0.1`

**日志示例：**
```
[配置] 加载用户过滤规则...
[配置]   deny 127.0.0.1
用户过滤规则: 1 条

[新连接] 客户端: 127.0.0.1:xxxxx
[用户过滤] 阻止用户访问: 127.0.0.1
```

---

### 测试5：网站引导（钓鱼）- 基础重定向

**目标：** 验证网站重定向功能

**前提：** 确保本地测试服务器在 `localhost:80` 运行

**配置 `redirect.txt`：**
```
baidu.com localhost
```

**步骤：**
1. 启动本地测试服务器（端口80）
2. 启动代理服务器
3. 访问 `http://www.baidu.com`
4. 观察浏览器地址栏和页面内容

**预期结果：**
- ✅ 启动时显示：网站引导规则: 1 条
- ✅ 浏览器地址栏显示：`www.baidu.com`
- ✅ 页面内容为本地测试服务器的内容
- ✅ 控制台显示：`[网站引导] www.baidu.com -> localhost:80`

**日志示例：**
```
[配置] 加载网站引导规则...
[配置]   baidu.com -> localhost
网站引导规则: 1 条

[新连接] 客户端: 127.0.0.1:xxxxx
[接收] 收到 xxx 字节的HTTP请求
[解析] 方法: GET, 主机: www.baidu.com:80, URL: /
[网站引导] www.baidu.com -> localhost:80
[连接] 成功连接到目标服务器: localhost:80
[转发] 已转发请求到目标服务器 (xxx 字节)
[响应] 状态码: 200
[完成] 已转发响应到客户端 (总计 xxx 字节)
```

**关键点：**
- 浏览器不知道被重定向，地址栏仍显示原始URL
- 这是透明代理，用户无感知
- 这就是钓鱼攻击的基本原理

---

### 测试6：网站引导 - 自定义端口

**目标：** 验证重定向到自定义端口

**准备：** 启动测试服务器在端口 8080

```bash
cd test_server/www
python -m http.server 8080
```

**配置 `redirect.txt`：**
```
qq.com localhost:8080
```

**步骤：**
1. 启动代理服务器
2. 访问 `http://www.qq.com`

**预期结果：**
- ✅ 浏览器地址栏显示：`www.qq.com`
- ✅ 实际连接到：`localhost:8080`
- ✅ 控制台显示：`[网站引导] www.qq.com -> localhost:8080`

---

### 测试7：网站引导 - 钓鱼页面演示

**目标：** 完整的钓鱼攻击演示

**准备钓鱼页面：**

1. 创建 `test_server/www/fake-bank.html`：

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Bank Login</title>
    <style>
        body {
            font-family: Arial;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
        }
        .login-box {
            background: white;
            padding: 40px;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
            width: 350px;
        }
        h1 {
            text-align: center;
            color: #333;
        }
        .warning {
            background: #fff3cd;
            border: 2px solid #ffc107;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            text-align: center;
        }
        .warning h3 {
            color: #d63384;
            margin: 0 0 10px 0;
        }
        input {
            width: 100%;
            padding: 12px;
            margin: 10px 0;
            border: 1px solid #ddd;
            border-radius: 5px;
            box-sizing: border-box;
        }
        button {
            width: 100%;
            padding: 12px;
            background: #667eea;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
        }
        button:hover {
            background: #764ba2;
        }
        .info {
            margin-top: 20px;
            padding: 15px;
            background: #e3f2fd;
            border-left: 4px solid #2196F3;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <div class="login-box">
        <h1>🏦 Bank Login</h1>

        <div class="warning">
            <h3>⚠️ 钓鱼测试成功！</h3>
            <p>这是一个模拟的钓鱼页面</p>
        </div>

        <form onsubmit="return showAlert()">
            <input type="text" placeholder="Username" required>
            <input type="password" placeholder="Password" required>
            <button type="submit">Login</button>
        </form>

        <div class="info">
            <strong>你以为访问的是：</strong><br>
            <span id="displayed-url" style="color: #d63384;"></span>
            <br><br>
            <strong>实际访问的是：</strong><br>
            <span style="color: #4CAF50;">localhost</span>
            <br><br>
            <small>这就是为什么你应该：</small>
            <ul style="text-align: left; margin: 10px 0;">
                <li>检查SSL证书</li>
                <li>注意浏览器警告</li>
                <li>警惕可疑链接</li>
            </ul>
        </div>
    </div>

    <script>
        document.getElementById('displayed-url').textContent = window.location.href;

        function showAlert() {
            alert('如果这是真实攻击，你的账号密码已被窃取！\n\n幸运的是，这只是一个安全演示。');
            return false;
        }
    </script>
</body>
</html>
```

2. 配置 `redirect.txt`：
```
bank-of-america.com localhost
chase.com localhost
wells-fargo.com localhost
```

3. 修改本地hosts文件（可选，用于测试）：
```
127.0.0.1 bank-of-america.com
127.0.0.1 chase.com
127.0.0.1 wells-fargo.com
```

**测试步骤：**
1. 启动测试服务器，确保 `fake-bank.html` 可访问
2. 启动代理服务器
3. 访问 `http://bank-of-america.com`（如果配置了hosts）
4. 或直接访问被重定向的域名

**预期结果：**
- ✅ 浏览器地址栏显示银行域名
- ✅ 实际显示模拟钓鱼页面
- ✅ 页面显示警告信息
- ✅ 演示了透明重定向的危险性

---

### 测试8：组合策略测试

**目标：** 验证多种功能同时工作

**配置：**

`website_filter.txt`：
```
deny qq.com
deny 163.com
```

`user_filter.txt`：
```
# 不配置任何规则（测试默认允许）
```

`redirect.txt`：
```
baidu.com localhost
google.com localhost
```

**测试矩阵：**

| 网站 | 预期行为 | 结果 |
|------|---------|------|
| www.baidu.com | 重定向到 localhost | ✅ |
| www.google.com | 重定向到 localhost | ✅ |
| www.qq.com | 被阻止 (403) | ✅ |
| www.163.com | 被阻止 (403) | ✅ |
| www.github.com | 正常访问 | ✅ |

**测试步骤：**
依次访问上述网站，记录结果。

**预期日志：**
```
[网站引导] www.baidu.com -> localhost:80     # 重定向
[网站引导] www.google.com -> localhost:80    # 重定向
[网站过滤] 阻止访问网站: www.qq.com          # 阻止
[网站过滤] 阻止访问网站: www.163.com         # 阻止
[连接] 成功连接到目标服务器: www.github.com # 正常
```

---

### 测试9：HTTP 缓存功能

**目标：** 验证缓存仍然正常工作

**配置：** 不配置任何过滤规则

**步骤：**
1. 启动代理服务器
2. 访问 `http://localhost/dynamic.php`（测试服务器）
3. 记录页面生成时间
4. 刷新页面（F5）
5. 检查时间是否改变

**预期结果：**
- ✅ 首次访问：时间显示（如 2025-01-15 14:30:25）
- ✅ 刷新后：时间不变（使用缓存）
- ✅ 控制台显示：
  - 首次：`[缓存] 未找到缓存` → `[缓存] 已保存缓存`
  - 刷新：`[缓存] 找到缓存` → `[缓存] 添加 If-Modified-Since` → `[响应] 状态码: 304`

**日志示例：**
```
# 首次请求
[缓存] 未找到缓存
[响应] 状态码: 200
[响应] Last-Modified: Mon, 15 Jan 2025 12:00:00 GMT
[缓存] 已保存缓存，大小: 3456 字节

# 第二次请求
[缓存] 找到缓存，大小: 3456 字节, Last-Modified: Mon, 15 Jan 2025 12:00:00 GMT
[缓存] 添加 If-Modified-Since: Mon, 15 Jan 2025 12:00:00 GMT
[响应] 状态码: 304
[缓存] 服务器返回304 Not Modified，缓存仍然有效
```

---

### 测试10：错误处理

**目标：** 验证各种错误情况的处理

#### 测试10.1：目标服务器不可达

**配置 `redirect.txt`：**
```
test.com localhost:9999
```

**步骤：**
1. 确保端口9999没有服务运行
2. 启动代理服务器
3. 访问 `http://test.com`

**预期结果：**
- ✅ 控制台显示：`[错误] 连接目标服务器 localhost:9999 失败`
- ✅ 浏览器显示连接错误或超时

#### 测试10.2：配置文件格式错误

**配置 `website_filter.txt`（故意错误）：**
```
deny
allow baidu.com without_action
wrongformat
```

**步骤：**
1. 启动代理服务器
2. 观察启动日志

**预期结果：**
- ✅ 忽略格式错误的行
- ✅ 仅加载正确格式的规则
- ✅ 启动成功（容错机制）

---

## 📊 测试报告模板

```
## 高级代理服务器测试报告

**测试时间：** 2025-01-15 14:00
**测试人员：** [你的姓名]
**环境：** Windows 10 + MinGW GCC

### 测试结果总结

| 测试项 | 状态 | 备注 |
|--------|------|------|
| 基础转发功能 | ✅ 通过 | 正常转发HTTP请求 |
| 网站过滤（单条规则） | ✅ 通过 | 成功阻止指定网站 |
| 网站过滤（多条规则） | ✅ 通过 | 多条规则正确匹配 |
| 用户过滤 | ✅ 通过 | 成功阻止指定IP |
| 网站引导（默认端口） | ✅ 通过 | 重定向到localhost:80 |
| 网站引导（自定义端口） | ✅ 通过 | 重定向到localhost:8080 |
| 钓鱼页面演示 | ✅ 通过 | 透明重定向成功 |
| 组合策略 | ✅ 通过 | 多种功能协同工作 |
| HTTP缓存 | ✅ 通过 | 缓存机制正常 |
| 错误处理 | ✅ 通过 | 容错机制有效 |

### 详细测试数据

#### 测试1：网站过滤
- 配置规则：`deny baidu.com`
- 访问 www.baidu.com：被阻止 ✅
- 访问 www.google.com：正常访问 ✅

#### 测试2：网站引导
- 配置规则：`baidu.com localhost`
- 浏览器地址栏：www.baidu.com ✅
- 实际内容：localhost测试页面 ✅
- 日志显示：`[网站引导] www.baidu.com -> localhost:80` ✅

#### 测试3：HTTP缓存
- 首次访问时间：2025-01-15 14:30:25
- 第2次刷新时间：2025-01-15 14:30:25 ✅
- 第3次刷新时间：2025-01-15 14:30:25 ✅
- 结论：缓存工作正常

### 控制台日志截图

[粘贴关键日志截图]

### 浏览器截图

[粘贴被阻止页面、重定向页面等截图]

### 遇到的问题

1. [记录测试中遇到的问题]
2. [问题描述...]

### 解决方法

1. [问题1的解决方法]
2. [问题2的解决方法]

### 改进建议

1. [对功能的改进建议]
2. [对性能的优化建议]

### 结论

所有核心功能测试通过，代理服务器工作正常。
```

---

## 🔧 调试技巧

### 1. 查看详细日志

代理服务器会输出详细的操作日志，包括：
- 连接信息
- 过滤决策
- 重定向操作
- 缓存操作
- 错误信息

**建议：** 使用日志重定向保存日志：
```bash
ProxyServerAdvanced.exe > proxy.log 2>&1
```

### 2. 使用浏览器开发者工具

按 F12 打开开发者工具，切换到 Network 标签：
- 查看请求/响应头
- 检查状态码
- 查看请求时间线

### 3. 使用 Wireshark 抓包

捕获本地回环流量（127.0.0.1），分析HTTP请求：
```
tcp.port == 10240
```

### 4. 清除缓存测试

删除缓存文件后重新测试：
```bash
del /Q cache\*.cache
```

### 5. 测试配置文件加载

在启动日志中检查：
```
[配置] 加载网站过滤规则...
[配置]   deny baidu.com
[配置]   deny qq.com
网站过滤规则: 2 条
```

确认规则已正确加载。

---

## ⚠️ 注意事项

### 1. 本地测试限制

- 某些网站可能无法通过代理访问（使用HTTPS）
- 代理不支持HTTPS，会自动忽略CONNECT请求
- 建议使用HTTP测试站点

### 2. 防火墙和安全软件

- 确保端口 10240 未被防火墙阻止
- 某些安全软件可能拦截代理操作

### 3. 浏览器缓存

- 测试前清除浏览器缓存
- 使用隐私模式/无痕模式测试
- 避免浏览器缓存干扰代理缓存测试

### 4. 配置文件编码

- 使用UTF-8编码保存配置文件
- 避免特殊字符和多余空格

### 5. 伦理和法律

- 网站引导（钓鱼）功能仅用于教育目的
- 不得用于非法攻击
- 测试时使用自己的测试环境

---

## 📞 故障排除

### 问题1：代理服务器无法启动

**可能原因：**
- 端口10240已被占用

**解决方法：**
```bash
# 检查端口占用
netstat -ano | findstr 10240

# 杀死占用进程
taskkill /PID <进程ID> /F
```

### 问题2：配置规则不生效

**可能原因：**
- 配置文件格式错误
- 配置文件路径不正确

**解决方法：**
1. 检查配置文件是否在代理服务器同一目录
2. 查看启动日志确认规则已加载
3. 检查文件编码（应为UTF-8）

### 问题3：网站引导后页面显示异常

**可能原因：**
- 目标服务器未运行
- 页面包含绝对URL

**解决方法：**
1. 确认测试服务器正在运行
2. 使用相对路径的测试页面
3. 检查控制台日志

### 问题4：浏览器显示"代理服务器拒绝连接"

**可能原因：**
- 代理服务器未启动
- 浏览器代理配置错误

**解决方法：**
1. 确认 `ProxyServerAdvanced.exe` 正在运行
2. 检查浏览器代理设置（127.0.0.1:10240）
3. 尝试重启浏览器

---

祝测试顺利！🎉

如有问题，请查看 `ADVANCED_README.md` 或联系开发者。
