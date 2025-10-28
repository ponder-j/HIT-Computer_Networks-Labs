# Lab4-2增强版 - 故障排查指南

## 问题：Phrolova有反应，Shorekeeper没反应

### 根本原因

**关键代码被注释掉了**！在转发器代码的第328行：

```c
// memcpy(eh->ether_dhost, entry->dest_mac, 6);  // ← 这行被注释了！
```

这导致以太网帧的目标MAC地址没有被更新：
- Phrolova收到数据包后，没有修改以太网帧头的目标MAC
- 转发出去的数据包目标MAC仍然是Phrolova自己的MAC
- Shorekeeper的网卡看到目标MAC不是自己，直接丢弃了数据包

### ✅ 已修复

我已经修复了所有三个转发器文件：
- `forward_raw_enhanced.c` (Phrolova)
- `forward_raw_enhanced_shorekeeper.c` (Shorekeeper)
- `forward_raw_enhanced_carlotta.c` (Carlotta)

**现在需要重新编译所有转发器程序！**

---

## 修复步骤

### 1️⃣ 重新编译转发器程序

```bash
cd Lab4-2_enhanced

# 编译Phrolova的转发器
gcc -o forward_raw_enhanced forward_raw_enhanced.c

# 编译Shorekeeper的转发器
gcc -o forward_raw_enhanced_shorekeeper forward_raw_enhanced_shorekeeper.c

# 编译Carlotta的转发器
gcc -o forward_raw_enhanced_carlotta forward_raw_enhanced_carlotta.c
```

### 2️⃣ 重新分发到各主机

```bash
# 分发到Phrolova
scp forward_raw_enhanced user@192.168.10.103:~/

# 分发到Shorekeeper
scp forward_raw_enhanced_shorekeeper user@192.168.10.101:~/

# 分发到Carlotta
scp forward_raw_enhanced_carlotta user@192.168.10.104:~/
```

### 3️⃣ 重启所有转发器

按顺序重启：

```bash
# 1. 在Phrolova上
sudo ./forward_raw_enhanced

# 2. 在Shorekeeper上
sudo ./forward_raw_enhanced_shorekeeper

# 3. 在Carlotta上
sudo ./forward_raw_enhanced_carlotta
```

### 4️⃣ 测试通信

在Camellya上发送测试消息，观察是否正常转发。

---

## 完整的故障排查清单

如果修复后仍有问题，按以下步骤逐一排查：

### ✅ 第1步：检查网络连通性

在每台主机上ping其他主机：

```bash
# 在Phrolova上
ping -c 3 192.168.10.101  # Shorekeeper
ping -c 3 192.168.10.104  # Carlotta
ping -c 3 192.168.10.102  # Jinhsi
```

**预期结果**：所有ping都应该成功

### ✅ 第2步：验证MAC地址配置

在每台主机上查看实际MAC地址：

```bash
ip a show ens33
```

对比代码中配置的MAC地址是否一致：

| 主机 | 配置的MAC | 实际MAC | 一致? |
|------|----------|---------|-------|
| Phrolova | 00:0c:29:86:81:ca | ? | ? |
| Shorekeeper | 00:50:56:22:13:0a | ? | ? |
| Carlotta | 00:50:56:3e:9d:96 | ? | ? |
| Jinhsi | 00:0c:29:69:e1:6b | ? | ? |

**如果不一致**：需要修改代码中的MAC地址配置并重新编译

### ✅ 第3步：检查程序启动顺序

正确的启动顺序：

```
1. Jinhsi (接收端)          ← 先启动
2. Carlotta (转发器3)
3. Shorekeeper (转发器2)
4. Phrolova (转发器1)
5. Camellya (发送端)         ← 最后启动
```

### ✅ 第4步：检查转发器日志

#### Phrolova的日志应该显示：

```
[时间] 收到数据包
  来源: 192.168.10.100
  目标: 192.168.10.102
  内容: "测试消息"
  原TTL: 255
  → 查找转发表: 找到目标 Next->Shorekeeper
  新TTL: 254
  ✓ 已转发到 Next->Shorekeeper (192.168.10.102)
  目标MAC: 00:50:56:22:13:0a
```

**关键点**：
- 必须显示"✓ 已转发"
- 目标MAC必须是Shorekeeper的MAC (00:50:56:22:13:0a)

#### Shorekeeper的日志应该显示：

```
[时间] 收到数据包
  来源: 192.168.10.100
  目标: 192.168.10.102
  内容: "测试消息"
  原TTL: 254
  → 查找转发表: 找到目标 Next->Carlotta
  新TTL: 253
  ✓ 已转发到 Next->Carlotta (192.168.10.102)
  目标MAC: 00:50:56:3e:9d:96
```

**如果Shorekeeper没有任何输出**：
- 说明没有收到数据包
- 检查Phrolova是否正常转发（查看上一步）
- 检查网络连通性

### ✅ 第5步：使用tcpdump抓包诊断

这是最强大的诊断工具！

#### 在Phrolova上抓包

```bash
# 在Phrolova上运行（新开一个终端）
sudo tcpdump -i ens33 -nn -e -v udp port 12345

# 或者抓取所有包
sudo tcpdump -i ens33 -nn -e -X
```

**预期输出**：
```
# 收到来自Camellya的包
12:34:56.123456 00:0c:29:c3:a7:63 > 00:0c:29:86:81:ca, ethertype IPv4, IP 192.168.10.100 > 192.168.10.102: UDP, length 13

# 转发给Shorekeeper的包
12:34:56.123457 00:0c:29:86:81:ca > 00:50:56:22:13:0a, ethertype IPv4, IP 192.168.10.100 > 192.168.10.102: UDP, length 13
```

**关键检查点**：
- 第一行：源MAC是Camellya，目标MAC是Phrolova ✅
- 第二行：源MAC是Phrolova，目标MAC是Shorekeeper ✅

**如果第二行的目标MAC不是Shorekeeper**：说明代码修复不完整，重新检查第328行

#### 在Shorekeeper上抓包

```bash
# 在Shorekeeper上运行
sudo tcpdump -i ens33 -nn -e -v udp port 12345
```

**预期输出**：
```
# 收到来自Phrolova的包
12:34:56.123458 00:0c:29:86:81:ca > 00:50:56:22:13:0a, ethertype IPv4, IP 192.168.10.100 > 192.168.10.102: UDP, length 13
```

**如果没有任何输出**：
- 说明数据包没有到达Shorekeeper
- 回到Phrolova检查是否正常转发
- 检查网络层（ping、路由表、ARP表）

### ✅ 第6步：检查ARP表

有时ARP缓存会导致问题：

```bash
# 查看ARP表
ip neigh show

# 清空ARP缓存（如果需要）
sudo ip neigh flush all
```

### ✅ 第7步：检查网卡混杂模式

原始套接字需要网卡处于混杂模式：

```bash
# 查看网卡状态
ip link show ens33

# 应该看到 PROMISC 标志
# 如果没有，手动设置：
sudo ip link set ens33 promisc on
```

### ✅ 第8步：检查防火墙规则

虽然原始套接字绕过UFW，但可能有其他防火墙规则：

```bash
# 检查iptables规则
sudo iptables -L -n -v

# 临时关闭防火墙测试（仅用于诊断）
sudo iptables -F
sudo iptables -X
```

### ✅ 第9步：检查SELinux/AppArmor

某些系统的安全模块可能阻止原始套接字：

```bash
# 检查SELinux状态
getenforce

# 临时禁用（仅用于诊断）
sudo setenforce 0

# 检查AppArmor
sudo aa-status
```

---

## 常见错误和解决方法

### 错误1：程序报"需要ROOT权限"

**原因**：原始套接字需要root权限

**解决方法**：
```bash
sudo ./forward_raw_enhanced
```

### 错误2：找不到网卡接口

**错误提示**：
```
错误：找不到网卡接口 ens33
```

**解决方法**：
```bash
# 查看实际网卡名称
ip a

# 如果不是ens33，修改代码中的INTERFACE宏定义
# 例如改为 eth0, enp0s3 等
```

### 错误3：转发器显示"未知目标"

**错误日志**：
```
✗ 未知目标，无法转发
```

**原因**：转发表中没有对应的IP地址

**解决方法**：
- 检查转发表配置中的目标IP是否正确（应该是192.168.10.102）
- 重新编译并部署

### 错误4：TTL过期

**错误日志**：
```
✗ TTL过期，丢弃数据包
```

**原因**：数据包经过太多跳或存在循环

**解决方法**：
- 检查是否存在转发循环
- 增加初始TTL值（在send程序中修改）

### 错误5：数据包发送失败

**错误日志**：
```
✗ 转发失败: Operation not permitted
```

**原因**：权限不足或网卡配置问题

**解决方法**：
```bash
# 确保使用sudo
sudo ./forward_raw_enhanced

# 检查网卡状态
ip link show ens33

# 确保网卡是UP状态
sudo ip link set ens33 up
```

---

## 使用wireshark进行深度诊断

如果以上方法都无法解决，使用Wireshark图形界面抓包：

### 在Phrolova上抓包

1. 安装Wireshark：
   ```bash
   sudo apt install wireshark
   ```

2. 启动Wireshark并选择ens33接口

3. 过滤条件：
   ```
   udp.port == 12345
   ```

4. 发送测试消息，观察抓包结果

5. 关键检查点：
   - 以太网帧头的源MAC和目标MAC
   - IP头的源IP和目标IP
   - TTL值的变化
   - UDP端口号

---

## 调试技巧

### 在代码中添加更多调试信息

在转发器代码中临时添加：

```c
// 在第327行后添加
printf("  [DEBUG] 修改前 - 目标MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
       eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
       eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]);

// 在第328行后添加
printf("  [DEBUG] 修改后 - 目标MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
       eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
       eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]);
```

重新编译并运行，查看MAC地址是否真的被修改了。

---

## 终极验证方案

按以下步骤进行完整测试：

### 第1步：最小化测试（2台主机）

先测试Camellya → Phrolova是否正常：

```bash
# 在Phrolova上运行简单的tcpdump
sudo tcpdump -i ens33 -nn -e udp port 12345

# 在Camellya上发送消息
# 观察Phrolova是否收到
```

### 第2步：扩展到3台主机

测试Camellya → Phrolova → Shorekeeper：

```bash
# 在Shorekeeper上运行tcpdump
sudo tcpdump -i ens33 -nn -e udp port 12345

# 在Phrolova上运行转发器
sudo ./forward_raw_enhanced

# 在Camellya上发送消息
# 观察Shorekeeper是否收到
```

### 第3步：完整测试（5台主机）

所有主机都启动，进行端到端测试。

---

## 快速诊断命令集合

```bash
# ===== 在所有主机上运行 =====

# 检查网络连通性
ping -c 3 192.168.10.100  # Camellya
ping -c 3 192.168.10.103  # Phrolova
ping -c 3 192.168.10.101  # Shorekeeper
ping -c 3 192.168.10.104  # Carlotta
ping -c 3 192.168.10.102  # Jinhsi

# 查看MAC地址
ip a show ens33

# 查看ARP表
ip neigh show

# 查看路由表
ip route show

# ===== 在转发器上运行 =====

# 抓包诊断（新开终端）
sudo tcpdump -i ens33 -nn -e -v udp port 12345

# 或者抓所有包
sudo tcpdump -i ens33 -nn -e -X

# 检查网卡混杂模式
ip link show ens33 | grep PROMISC

# 设置混杂模式
sudo ip link set ens33 promisc on
```

---

## 成功的标志

当一切正常时，你应该看到：

### Phrolova的日志：
```
✓ 已转发到 Next->Shorekeeper (192.168.10.102)
目标MAC: 00:50:56:22:13:0a
```

### Shorekeeper的日志：
```
✓ 已转发到 Next->Carlotta (192.168.10.102)
目标MAC: 00:50:56:3e:9d:96
```

### Carlotta的日志：
```
✓ 已转发到 Jinhsi (192.168.10.102)
目标MAC: 00:0c:29:69:e1:6b
```

### Jinhsi的日志：
```
╔════════════════════════════════════════════════════════════════╗
║ 消息 #1                                            [12:34:56] ║
╠════════════════════════════════════════════════════════════════╣
║ 来源主机: Camellya      (192.168.10.100)                      ║
║ 消息内容:                                                      ║
║ 测试消息                                                       ║
╚════════════════════════════════════════════════════════════════╝
```

**TTL的变化**：255 → 254 → 253 → 252

如果看到以上输出，恭喜你，实验成功！🎉
