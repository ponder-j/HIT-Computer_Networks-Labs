# Lab4-2增强版 - UFW防火墙配置指南

## 网络拓扑与通信方式

```
Camellya → Phrolova → Shorekeeper → Carlotta → Jinhsi
(原始套接字) (原始套接字) (原始套接字) (原始套接字) (UDP:12345)
```

## 重要说明

⚠️ **原始套接字与防火墙的关系**：

1. **发送端和转发器**使用 `AF_PACKET` 原始套接字（链路层）
   - 工作在以太网帧层面
   - **绕过传统防火墙的端口规则**
   - 不受UFW端口规则限制

2. **接收端**使用 `AF_INET` UDP套接字（传输层）
   - 监听UDP端口12345
   - **受UFW端口规则控制**

## UFW配置方案

### 1️⃣ Camellya (发送端) - 192.168.10.100

**通信方式**：使用原始套接字发送（不需要开放端口）

**UFW配置**：
```bash
# 可以保持默认策略
sudo ufw status
```

**无需特殊配置**，因为：
- 只发送，不监听端口
- 使用原始套接字，绕过UFW端口规则

---

### 2️⃣ Phrolova (转发器1) - 192.168.10.103

**通信方式**：使用原始套接字接收和转发（不需要开放端口）

**UFW配置**：
```bash
# 可以保持默认策略
sudo ufw status
```

**无需特殊配置**，因为：
- 原始套接字直接捕获以太网帧
- 不通过传统的TCP/UDP端口

---

### 3️⃣ Shorekeeper (转发器2) - 192.168.10.101

**通信方式**：使用原始套接字接收和转发（不需要开放端口）

**UFW配置**：
```bash
# 可以保持默认策略
sudo ufw status
```

**无需特殊配置**（同Phrolova）

---

### 4️⃣ Carlotta (转发器3) - 192.168.10.104

**通信方式**：使用原始套接字接收和转发（不需要开放端口）

**UFW配置**：
```bash
# 可以保持默认策略
sudo ufw status
```

**无需特殊配置**（同Phrolova）

---

### 5️⃣ Jinhsi (接收端) - 192.168.10.102 ⚠️ 重要！

**通信方式**：使用UDP套接字监听端口12345（需要开放端口）

**UFW配置**（必需）：

```bash
# 1. 允许来自Carlotta的UDP 12345端口（推荐 - 最安全）
sudo ufw allow from 192.168.10.104 to any port 12345 proto udp

# 或者，允许来自整个实验网段（次选）
sudo ufw allow from 192.168.10.0/24 to any port 12345 proto udp

# 或者，允许所有来源的UDP 12345端口（不推荐 - 仅测试用）
sudo ufw allow 12345/udp

# 2. 启用UFW（如果还未启用）
sudo ufw enable

# 3. 查看规则
sudo ufw status numbered
```

**推荐配置**：
```bash
sudo ufw allow from 192.168.10.104 to any port 12345 proto udp
sudo ufw enable
```

---

## 完整配置脚本

### Jinhsi主机（接收端）配置脚本

```bash
#!/bin/bash
# 在Jinhsi主机上运行

echo "配置UFW防火墙..."

# 检查UFW状态
sudo ufw status

# 允许来自Carlotta的UDP 12345端口
sudo ufw allow from 192.168.10.104 to any port 12345 proto udp comment 'Lab4-2 UDP接收端口'

# 如果需要SSH远程访问，确保SSH端口开放
sudo ufw allow ssh

# 启用UFW
sudo ufw --force enable

# 显示配置结果
echo "UFW配置完成！"
sudo ufw status numbered
```

保存为 `setup_ufw_jinhsi.sh`，然后运行：
```bash
chmod +x setup_ufw_jinhsi.sh
sudo ./setup_ufw_jinhsi.sh
```

---

## 为什么其他主机不需要配置UFW？

### 原始套接字的工作原理

```
┌─────────────────────────────────────────┐
│          应用层 (Application)            │
├─────────────────────────────────────────┤
│     传输层 (TCP/UDP) ← UFW在这里工作    │  UDP套接字 (Jinhsi)
├─────────────────────────────────────────┤
│        网络层 (IP)                       │
├─────────────────────────────────────────┤
│     链路层 (Ethernet) ← 原始套接字工作   │  原始套接字 (发送端+转发器)
├─────────────────────────────────────────┤
│        物理层 (Physical)                 │
└─────────────────────────────────────────┘
```

- **原始套接字**（`AF_PACKET`）在链路层工作，直接捕获和发送以太网帧
- **UFW防火墙**在网络层和传输层工作，控制IP/TCP/UDP流量
- 因此，**原始套接字绕过UFW的端口规则**

### 但是需要注意权限

原始套接字需要：
- **ROOT权限**（`sudo`）
- **CAP_NET_RAW能力**

```bash
# 检查程序是否有权限
sudo ./forward_raw_enhanced  # ✅ 正确
./forward_raw_enhanced       # ❌ 会失败（权限不足）
```

---

## UFW配置验证

### 在Jinhsi主机上验证

```bash
# 1. 检查UFW状态
sudo ufw status verbose

# 应该看到：
# To                         Action      From
# --                         ------      ----
# 12345/udp                  ALLOW       192.168.10.104

# 2. 测试端口是否开放
sudo netstat -ulnp | grep 12345

# 应该看到：
# udp        0      0 0.0.0.0:12345           0.0.0.0:*
```

### 使用nmap测试（可选）

从Carlotta主机测试Jinhsi：
```bash
# 在Carlotta上运行
nmap -sU -p 12345 192.168.10.102

# 应该看到：
# PORT      STATE         SERVICE
# 12345/udp open|filtered unknown
```

---

## 常见问题排查

### 问题1：Jinhsi收不到消息，但转发器正常

**可能原因**：UFW阻止了UDP 12345端口

**解决方法**：
```bash
# 在Jinhsi上运行
sudo ufw allow 12345/udp
sudo ufw reload
```

### 问题2：UFW规则不生效

**解决方法**：
```bash
# 重启UFW
sudo ufw disable
sudo ufw enable

# 或者重载规则
sudo ufw reload
```

### 问题3：如何查看UFW日志

```bash
# 启用日志
sudo ufw logging on

# 查看日志
sudo tail -f /var/log/ufw.log
```

### 问题4：如何删除UFW规则

```bash
# 查看规则编号
sudo ufw status numbered

# 删除指定编号的规则（例如删除规则1）
sudo ufw delete 1
```

---

## 安全建议

### 生产环境配置

如果在生产环境部署，建议：

1. **最小化开放端口**：
   ```bash
   # 只允许来自特定IP的特定端口
   sudo ufw default deny incoming
   sudo ufw default allow outgoing
   sudo ufw allow from 192.168.10.104 to any port 12345 proto udp
   ```

2. **限制连接速率**：
   ```bash
   # 限制每分钟最多30个连接
   sudo ufw limit 12345/udp
   ```

3. **启用日志记录**：
   ```bash
   sudo ufw logging medium
   ```

### 实验环境配置

实验环境可以简化：
```bash
# Jinhsi主机
sudo ufw allow from 192.168.10.0/24 to any port 12345 proto udp
```

---

## 与iptables的关系

UFW是iptables的前端，实际规则存储在iptables中：

```bash
# 查看底层iptables规则
sudo iptables -L -n -v

# 查看UFW的iptables规则
sudo iptables -L ufw-user-input -n -v
```

---

## 快速配置命令汇总

```bash
# ===== 仅在Jinhsi主机上执行 =====

# 允许UDP 12345端口（来自Carlotta）
sudo ufw allow from 192.168.10.104 to any port 12345 proto udp

# 启用UFW
sudo ufw enable

# 查看状态
sudo ufw status numbered

# ===== 其他主机无需配置UFW =====
# Camellya、Phrolova、Shorekeeper、Carlotta 都使用原始套接字
# 不受UFW端口规则限制，无需特殊配置
```

---

## 总结

| 主机 | 需要配置UFW | 原因 |
|------|-----------|------|
| Camellya | ❌ | 使用原始套接字发送，绕过UFW |
| Phrolova | ❌ | 使用原始套接字接收/转发，绕过UFW |
| Shorekeeper | ❌ | 使用原始套接字接收/转发，绕过UFW |
| Carlotta | ❌ | 使用原始套接字接收/转发，绕过UFW |
| Jinhsi | ✅ **必需** | 使用UDP套接字监听12345端口 |

**关键点**：
- 只有Jinhsi需要配置UFW开放UDP 12345端口
- 其他主机使用原始套接字，不受UFW端口规则限制
- 所有使用原始套接字的程序都需要ROOT权限运行

---

配置完成后，记得测试整个通信链路是否正常！🎉
