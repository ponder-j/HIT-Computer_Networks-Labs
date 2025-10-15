# 快速开始指南

## ⚡ 5分钟快速演示

### 步骤1：编译（30秒）

```bash
cd D:\Grade3_Courses\Computer_Networks_Labs\Lab1
compile_advanced.bat
```

### 步骤2：配置规则（1分钟）

编辑 `website_filter.txt`，添加：
```
deny baidu.com
```

编辑 `redirect.txt`，添加：
```
qq.com localhost
```

### 步骤3：启动测试服务器（30秒）

在 test_server/www 目录下：
```bash
python -m http.server 80
```

### 步骤4：启动代理（10秒）

```bash
ProxyServerAdvanced.exe
```

### 步骤5：配置浏览器（1分钟）

Firefox/Chrome 设置：
- HTTP代理: `127.0.0.1`
- 端口: `10240`

### 步骤6：测试（2分钟）

1. 访问 `http://www.baidu.com` → 应该被阻止 ❌
2. 访问 `http://www.qq.com` → 应该重定向到localhost ✅
3. 访问 `http://www.github.com` → 正常访问 ✅

完成！🎉

---

## 📝 常用配置示例

### 场景1：阻止社交媒体

`website_filter.txt`:
```
deny facebook.com
deny twitter.com
deny instagram.com
```

### 场景2：钓鱼演示

`redirect.txt`:
```
bank.com localhost:80
paypal.com localhost:80
```

### 场景3：用户白名单

`user_filter.txt`:
```
allow 192.168.1.1
deny 192.168.1.100
```

---

## 🐛 快速调试

**问题：** 代理启动失败
**解决：** `netstat -ano | findstr 10240` 检查端口占用

**问题：** 规则不生效
**解决：** 查看启动日志，确认规则已加载

**问题：** 浏览器连接失败
**解决：** 确认代理地址 `127.0.0.1:10240`

---

## 📚 更多信息

- 详细文档：`ADVANCED_README.md`
- 测试指南：`TESTING_ADVANCED.md`
- 原理说明：查看源码注释

---

Have fun! 🚀
