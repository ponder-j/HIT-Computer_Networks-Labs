# Lab4-2增强版 - 5台主机串行转发配置指南

## 网络拓扑

```
发送端 → 转发器1 → 转发器2 → 转发器3 → 接收端
Camellya → Phrolova → Shorekeeper → Carlotta → Jinhsi
.100       .103        .101          .104        .102
```

**数据流向**：Camellya 发送数据，经过 3 台转发器中转，最终到达 Jinhsi

## 主机配置表

| 角色 | 主机名 | IP地址 | MAC地址 | 程序文件 | ROOT权限 |
|------|--------|--------|---------|----------|----------|
| 发送端 | Camellya | 192.168.10.100 | 00:0c:29:c3:a7:63 | send_raw_enhanced | ✓ |
| 转发器1 | Phrolova | 192.168.10.103 | 00:0c:29:86:81:ca | forward_raw_enhanced | ✓ |
| 转发器2 | Shorekeeper | 192.168.10.101 | 00:50:56:22:13:0a | forward_raw_enhanced_shorekeeper | ✓ |
| 转发器3 | Carlotta | 192.168.10.104 | 00:50:56:3e:9d:96 | forward_raw_enhanced_carlotta | ✓ |
| 接收端 | Jinhsi | 192.168.10.102 | 00:0c:29:69:e1:6b | recv_raw_enhanced | ✗ |

## 关键配置说明

### 1. 发送端 (Camellya)

**程序**：`send_raw_enhanced.c`

**关键配置**：
- 第一跳目标：Phrolova (192.168.10.103)
- 以太网目标MAC：Phrolova的MAC (00:0c:29:86:81:ca)
- IP目标地址：Jinhsi (192.168.10.102) - 最终目标

### 2. 转发器1 (Phrolova)

**程序**：`forward_raw_enhanced.c`

**转发表配置**：
```
目标IP: 192.168.10.102 (Jinhsi)
下一跳MAC: 00:50:56:22:13:0a (Shorekeeper)
```

### 3. 转发器2 (Shorekeeper)

**程序**：`forward_raw_enhanced_shorekeeper.c`

**转发表配置**：
```
目标IP: 192.168.10.102 (Jinhsi)
下一跳MAC: 00:50:56:3e:9d:96 (Carlotta)
```

### 4. 转发器3 (Carlotta)

**程序**：`forward_raw_enhanced_carlotta.c`

**转发表配置**：
```
目标IP: 192.168.10.102 (Jinhsi)
下一跳MAC: 00:0c:29:69:e1:6b (Jinhsi) - 最后一跳，直接送达
```

### 5. 接收端 (Jinhsi)

**程序**：`recv_raw_enhanced.c`

**配置**：
- 监听UDP端口：12345
- 已知发送端：Camellya (192.168.10.100)

## 编译与部署步骤

### 第1步：编译所有程序

在Lab4-2_enhanced目录下：

```bash
# 编译发送端
gcc -o send_raw_enhanced send_raw_enhanced.c

# 编译转发器1 (Phrolova)
gcc -o forward_raw_enhanced forward_raw_enhanced.c

# 编译转发器2 (Shorekeeper)
gcc -o forward_raw_enhanced_shorekeeper forward_raw_enhanced_shorekeeper.c

# 编译转发器3 (Carlotta)
gcc -o forward_raw_enhanced_carlotta forward_raw_enhanced_carlotta.c

# 编译接收端
gcc -o recv_raw_enhanced recv_raw_enhanced.c
```

### 第2步：分发程序到各主机

```bash
# 发送到Camellya (192.168.10.100)
scp send_raw_enhanced user@192.168.10.100:~/

# 发送到Phrolova (192.168.10.103)
scp forward_raw_enhanced user@192.168.10.103:~/

# 发送到Shorekeeper (192.168.10.101)
scp forward_raw_enhanced_shorekeeper user@192.168.10.101:~/

# 发送到Carlotta (192.168.10.104)
scp forward_raw_enhanced_carlotta user@192.168.10.104:~/

# 发送到Jinhsi (192.168.10.102)
scp recv_raw_enhanced user@192.168.10.102:~/
```

### 第3步：按顺序启动程序

⚠️ **重要**：必须按以下顺序启动！

#### 1. Jinhsi主机（接收端）- 先启动
```bash
./recv_raw_enhanced
```

#### 2. Carlotta主机（转发器3）
```bash
sudo ./forward_raw_enhanced_carlotta
```

#### 3. Shorekeeper主机（转发器2）
```bash
sudo ./forward_raw_enhanced_shorekeeper
```

#### 4. Phrolova主机（转发器1）
```bash
sudo ./forward_raw_enhanced
```

#### 5. Camellya主机（发送端）- 最后启动
```bash
sudo ./send_raw_enhanced
```

## 数据包转发流程详解

### 阶段1：Camellya发送数据包

```
┌─────────────────────────────────────┐
│ 以太网帧头                            │
│ 源MAC: 00:0c:29:c3:a7:63 (Camellya) │
│ 目标MAC: 00:0c:29:86:81:ca (Phrolova)│
├─────────────────────────────────────┤
│ IP头                                 │
│ 源IP: 192.168.10.100                │
│ 目标IP: 192.168.10.102 (Jinhsi)     │
│ TTL: 255                            │
├─────────────────────────────────────┤
│ UDP头 + 数据                         │
└─────────────────────────────────────┘
```

### 阶段2：Phrolova转发（第1跳）

- 收到数据包，提取目标IP: 192.168.10.102
- 查找转发表 → 下一跳：Shorekeeper
- 修改以太网目标MAC为Shorekeeper的MAC
- TTL递减：255 → 254
- 转发给Shorekeeper

### 阶段3：Shorekeeper转发（第2跳）

- 收到数据包，提取目标IP: 192.168.10.102
- 查找转发表 → 下一跳：Carlotta
- 修改以太网目标MAC为Carlotta的MAC
- TTL递减：254 → 253
- 转发给Carlotta

### 阶段4：Carlotta转发（第3跳）

- 收到数据包，提取目标IP: 192.168.10.102
- 查找转发表 → 下一跳：Jinhsi（最终目标）
- 修改以太网目标MAC为Jinhsi的MAC
- TTL递减：253 → 252
- 转发给Jinhsi

### 阶段5：Jinhsi接收

```
┌─────────────────────────────────────┐
│ 以太网帧头                            │
│ 源MAC: 00:50:56:3e:9d:96 (Carlotta) │
│ 目标MAC: 00:0c:29:69:e1:6b (Jinhsi) │
├─────────────────────────────────────┤
│ IP头                                 │
│ 源IP: 192.168.10.100 (Camellya)     │← 保持不变
│ 目标IP: 192.168.10.102 (Jinhsi)     │← 保持不变
│ TTL: 252                            │← 递减了3次
├─────────────────────────────────────┤
│ UDP头 + 数据                         │
└─────────────────────────────────────┘
```

Jinhsi接收UDP数据包，显示消息。

## 关键观察点

### IP地址变化
- **源IP**：始终是 192.168.10.100 (Camellya)
- **目标IP**：始终是 192.168.10.102 (Jinhsi)
- ✅ **IP地址在整个转发过程中保持不变**

### MAC地址变化
每一跳的MAC地址都会改变：
1. Camellya → Phrolova: 目标MAC = Phrolova
2. Phrolova → Shorekeeper: 目标MAC = Shorekeeper
3. Shorekeeper → Carlotta: 目标MAC = Carlotta
4. Carlotta → Jinhsi: 目标MAC = Jinhsi
- ✅ **MAC地址在每一跳都会更新**

### TTL变化
- 初始：255
- 经过Phrolova：254
- 经过Shorekeeper：253
- 经过Carlotta：252
- 到达Jinhsi：252
- ✅ **每经过一个转发器，TTL递减1**

## 测试步骤

1. **验证网络连通性**：在各主机上ping其他主机
   ```bash
   ping 192.168.10.100  # Camellya
   ping 192.168.10.103  # Phrolova
   ping 192.168.10.101  # Shorekeeper
   ping 192.168.10.104  # Carlotta
   ping 192.168.10.102  # Jinhsi
   ```

2. **启动所有程序**：按上述顺序启动

3. **在Camellya上发送消息**：
   - 选择接收端 Jinhsi (选项1)
   - 输入测试消息，例如："Hello from Camellya!"

4. **在Jinhsi上查看接收**：应显示来自Camellya的消息

5. **观察转发器日志**：
   - Phrolova应显示：收到来自Camellya，转发到Shorekeeper
   - Shorekeeper应显示：收到数据包，转发到Carlotta
   - Carlotta应显示：收到数据包，转发到Jinhsi

6. **验证TTL递减**：在每个转发器的日志中观察TTL值

## 故障排查

### 问题1：接收端收不到消息

**可能原因**：
- 某个转发器没有启动
- MAC地址配置错误
- 转发表配置错误

**排查步骤**：
1. 检查每个转发器是否正常运行
2. 在转发器上运行 `ip a` 验证MAC地址
3. 检查转发表配置是否与实际MAC匹配

### 问题2：TTL过期

**可能原因**：
- 存在循环转发
- 初始TTL设置过小

**解决方法**：
- 检查转发表配置，确保没有循环
- 增加初始TTL值（当前为255）

### 问题3：转发器报错"找不到网卡接口"

**解决方法**：
```bash
# 查看网卡名称
ip a

# 如果网卡不是ens33，需要修改代码中的INTERFACE宏定义
```

## 实验报告要点

1. **网络拓扑图**：画出5台主机的串行拓扑
2. **数据包流向**：描述数据包经过的路径
3. **转发表内容**：展示每个转发器的转发表
4. **TTL变化**：记录每一跳的TTL值
5. **MAC地址变化**：记录每一跳的源/目标MAC
6. **IP地址不变性**：验证IP地址在转发过程中保持不变
7. **截图**：
   - 发送端发送消息的截图
   - 3个转发器的转发日志截图
   - 接收端接收消息的截图

## 与Lab4-2原版的区别

| 特性 | Lab4-2原版 | Lab4-2增强版（5主机） |
|------|-----------|---------------------|
| 拓扑结构 | 星型（1个中心转发器） | 串行（3个串联转发器） |
| 主机数量 | 3-5台 | 5台（固定） |
| 转发跳数 | 1跳 | 3跳 |
| TTL递减 | 1 | 3 |
| 转发表 | 多条目（多个接收端） | 单条目（单个下一跳） |
| 转发决策 | 基于目标IP查表 | 基于目标IP查找下一跳 |
| 实验难度 | 中等 | 较高 |

## 扩展思考

1. **如果增加到7台主机（5个转发器）**，应如何配置？
2. **如何实现双向通信**（Jinhsi也能发送给Camellya）？
3. **如何处理多个发送端和多个接收端的场景**？
4. **能否实现动态路由**（自动学习转发表）？

---

**配置完成！祝实验顺利！** 🎉
