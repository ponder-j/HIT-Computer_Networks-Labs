# Lab4-2 与 Lab4-2_enhanced 代码对比分析

## 一、网络拓扑结构差异

### Lab4-2（原版）

```
Camellya (发送端) → Shorekeeper (转发器) → Jinhsi (接收端)
   .100                  .101                    .102
```

- **拓扑类型**：星型（单中心转发器）
- **主机数量**：3台
- **转发跳数**：1跳
- **TTL递减**：255 → 254

### Lab4-2_enhanced（增强版）

```
Camellya → Phrolova → Shorekeeper → Carlotta → Jinhsi
  .100       .103         .101         .104       .102
(发送端)   (转发器1)    (转发器2)     (转发器3)   (接收端)
```

- **拓扑类型**：串行（链式转发）
- **主机数量**：5台
- **转发跳数**：3跳
- **TTL递减**：255 → 254 → 253 → 252

---

## 二、发送端程序对比

| 特性 | Lab4-2 | Lab4-2_enhanced |
|------|--------|-----------------|
| **目标MAC配置** | 硬编码为 Shorekeeper<br/>(00:50:56:22:13:0a) | 硬编码为 Phrolova<br/>(00:0c:29:86:81:ca) |
| **目标IP配置** | 固定为 Jinhsi<br/>(192.168.10.102) | 固定为 Jinhsi<br/>(192.168.10.102) |
| **多目标支持** | ❌ 不支持 | ✅ 支持（通过 targets 数组） |
| **用户交互** | 简单输入 | 菜单式选择 + 输入 |
| **代码行数** | 232 行 | 281 行 |

### 关键代码差异

**Lab4-2_enhanced 支持多目标选择**（send_raw_enhanced.c:48-88）：

```c
// 目标主机配置（最终接收端）
typedef struct {
    char name[32];
    char ip[16];
} TargetHost;

TargetHost targets[] = {
    {"Jinhsi", "192.168.10.102"}
};

int select_target() {
    // 显示目标选择菜单
    printf("\n请选择接收端：\n");
    for (int i = 0; i < num_targets; i++) {
        printf("  [%d] %s (%s)\n", i + 1, targets[i].name, targets[i].ip);
    }
    // ...
}
```

---

## 三、转发器程序对比（核心差异）

### 3.1 转发机制

#### Lab4-2（forward_raw.c）

- **转发策略**：硬编码，直接转发到 Jinhsi
- **配置方式**：通过宏定义固定MAC地址

```c
// forward_raw.c:38-44
#define DEST_MAC0 0x00  // Jinhsi的MAC
#define DEST_MAC1 0x0c
#define DEST_MAC2 0x29
#define DEST_MAC3 0x69
#define DEST_MAC4 0xe1
#define DEST_MAC5 0x6b
```

#### Lab4-2_enhanced（forward_raw_enhanced.c）

- **转发策略**：基于转发表（Forwarding Table）查表转发
- **配置方式**：通过转发表数据结构

```c
// forward_raw_enhanced.c:35-40
typedef struct {
    char dest_ip[16];           // 目标IP地址
    unsigned char dest_mac[6];  // 下一跳MAC地址
    char description[32];        // 描述信息
    int packet_count;            // 转发统计
} ForwardEntry;

// forward_raw_enhanced.c:82-102
void init_forward_table() {
    // 本配置用于: Phrolova (192.168.10.103) - 第一跳转发器
    // 数据流: Camellya → Phrolova → Shorekeeper → Carlotta → Jinhsi

    // 目标Jinhsi，下一跳是Shorekeeper
    strcpy(forward_table[0].dest_ip, "192.168.10.102");  // 最终目标IP
    forward_table[0].dest_mac[0] = 0x00;  // Shorekeeper的MAC
    forward_table[0].dest_mac[1] = 0x50;
    forward_table[0].dest_mac[2] = 0x56;
    forward_table[0].dest_mac[3] = 0x22;
    forward_table[0].dest_mac[4] = 0x13;
    forward_table[0].dest_mac[5] = 0x0a;
    strcpy(forward_table[0].description, "Next->Shorekeeper");
    forward_table[0].packet_count = 0;

    forward_table_size = 1;
}
```

### 3.2 转发表查找机制

**Lab4-2_enhanced 增加的功能**（forward_raw_enhanced.c:105-112）：

```c
ForwardEntry* lookup_forward_table(const char *dest_ip) {
    for (int i = 0; i < forward_table_size; i++) {
        if (strcmp(forward_table[i].dest_ip, dest_ip) == 0) {
            return &forward_table[i];  // 返回匹配的转发条目
        }
    }
    return NULL;  // 未找到目标
}
```

这实现了**逐跳转发**（hop-by-hop forwarding）：
- **Phrolova**：查表发现目标192.168.10.102，下一跳→Shorekeeper
- **Shorekeeper**：查表发现目标192.168.10.102，下一跳→Carlotta
- **Carlotta**：查表发现目标192.168.10.102，下一跳→Jinhsi（最后一跳）

### 3.3 循环检测

#### Lab4-2

- ❌ 无循环检测

#### Lab4-2_enhanced

✅ **源MAC检测**（forward_raw_enhanced.c:245-247）：
```c
// 如果数据包的源MAC是本机MAC，则忽略（避免循环捕获）
if (memcmp(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, 6) == 0) {
    continue;
}
```

✅ **目标IP检测**（forward_raw_enhanced.c:269-279）：
```c
// 忽略发给自己的包
struct ifreq ifr;
memset(&ifr, 0, sizeof(ifr));
strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ - 1);
if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
    struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    char my_ip[INET_ADDRSTRLEN];
    strcpy(my_ip, inet_ntoa(ipaddr->sin_addr));
    if (strcmp(dst_ip, my_ip) == 0) {
        continue;  // 忽略发给自己的包
    }
}
```

### 3.4 统计信息

#### Lab4-2

```c
static int msg_count = 0;  // 简单计数
```

#### Lab4-2_enhanced

```c
// forward_raw_enhanced.c:47-54
typedef struct {
    int total_received;   // 总接收数
    int total_forwarded;  // 成功转发数
    int unknown_dest;     // 未知目标数
    int ttl_expired;      // TTL过期数
} ForwardStats;

// forward_raw_enhanced.c:139-159
void print_statistics() {
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                         转发统计信息                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ 总接收数据包: %-10d                                       ║\n", stats.total_received);
    printf("║ 成功转发数: %-10d                                         ║\n", stats.total_forwarded);
    printf("║ 未知目标数: %-10d                                         ║\n", stats.unknown_dest);
    printf("║ TTL过期数: %-10d                                          ║\n", stats.ttl_expired);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║                    各目标转发统计                              ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");

    for (int i = 0; i < forward_table_size; i++) {
        printf("║ %-15s (%9s): %-10d                        ║\n",
               forward_table[i].dest_ip,
               forward_table[i].description,
               forward_table[i].packet_count);
    }

    printf("╚════════════════════════════════════════════════════════════════╝\n");
}
```

---

## 四、接收端程序对比

### Lab4-2（recv_raw.c）

- 简单的消息框显示
- 基础的消息计数
- 无发送端识别

### Lab4-2_enhanced（recv_raw_enhanced.c）

✅ **已知发送端识别**（通过IP→名称映射）：
```c
// recv_raw_enhanced.c:27-36
typedef struct {
    char ip[16];
    char name[32];
} KnownHost;

KnownHost known_senders[] = {
    {"192.168.10.100", "Camellya"}
};

// recv_raw_enhanced.c:64-71
const char* get_sender_name(const char *ip) {
    for (int i = 0; i < num_known_senders; i++) {
        if (strcmp(known_senders[i].ip, ip) == 0) {
            return known_senders[i].name;
        }
    }
    return "Unknown";
}
```

✅ **详细的统计信息**：
```c
// recv_raw_enhanced.c:39-45
typedef struct {
    int total_received;    // 总接收消息数
    int from_camellya;     // 来自Camellya的消息数
    int from_others;       // 来自其他主机的消息数
} RecvStats;

// recv_raw_enhanced.c:85-93
void print_statistics() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                         接收统计信息                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ 总接收消息数: %-10d                                       ║\n", stats.total_received);
    printf("║ 来自Camellya: %-10d                                       ║\n", stats.from_camellya);
    printf("║ 来自其他:     %-10d                                       ║\n", stats.from_others);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}
```

✅ **更友好的界面**（双线边框，主机名显示）

---

## 五、关键技术差异总结

| 技术特性 | Lab4-2 | Lab4-2_enhanced |
|---------|--------|-----------------|
| **转发决策** | 硬编码MAC | 转发表查找 |
| **可扩展性** | 低（需修改代码） | 高（修改转发表即可） |
| **循环检测** | 无 | 有（源MAC + 目标IP） |
| **TTL处理** | 递减1次 | 递减3次（3跳） |
| **统计功能** | 基础 | 详细（多维度统计） |
| **错误处理** | 基础 | 完善（TTL过期、未知目标） |
| **代码复用** | 低 | 高（通用转发器代码） |
| **程序数量** | 3个 | 5个（含3个转发器变体） |
| **转发表支持** | ❌ | ✅ |
| **多目标支持** | ❌ | ✅ |

---

## 六、实验目的差异

### Lab4-2

- 理解**单跳转发**的基本原理
- 掌握原始套接字的使用
- 理解MAC地址、IP地址的作用

### Lab4-2_enhanced

- 理解**多跳转发**的路由机制
- 掌握**转发表**的设计与实现
- 理解**逐跳转发**（每个路由器独立决策）
- 验证**IP地址不变、MAC地址逐跳更新**的原理
- 理解**TTL机制**防止循环转发

---

## 七、关键代码行对比

### 转发器的核心差异

```c
// ====== Lab4-2: 硬编码转发 ======
// forward_raw.c:189-194
eh->ether_dhost[0] = DEST_MAC0;  // 直接设置为Jinhsi的MAC
eh->ether_dhost[1] = DEST_MAC1;
eh->ether_dhost[2] = DEST_MAC2;
eh->ether_dhost[3] = DEST_MAC3;
eh->ether_dhost[4] = DEST_MAC4;
eh->ether_dhost[5] = DEST_MAC5;


// ====== Lab4-2_enhanced: 查表转发 ======
// forward_raw_enhanced.c:316-328
ForwardEntry *entry = lookup_forward_table(dst_ip);  // 查找转发表

if (entry == NULL) {
    printf("  ✗ 未知目标，无法转发\n\n");
    stats.unknown_dest++;
    continue;
}

printf("  → 查找转发表: 找到目标 %s\n", entry->description);

// 修改以太网帧头：更新目标MAC地址和源MAC地址
memcpy(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, 6);
memcpy(eh->ether_dhost, entry->dest_mac, 6);  // 设置为下一跳MAC
```

### 体现的核心原理

这体现了**路由器转发表机制**的核心思想：

- **不是**：根据目标IP找到目标主机的MAC
- **而是**：根据目标IP找到**下一跳**的MAC

这是路由器与交换机的根本区别！

---

## 八、文件对应关系

```
Lab4-2/
├── send_raw.c              → Lab4-2_enhanced/send_raw_enhanced.c
├── recv_raw.c              → Lab4-2_enhanced/recv_raw_enhanced.c
└── forward_raw.c           → Lab4-2_enhanced/forward_raw_enhanced.c
    (Shorekeeper专用)          (Phrolova专用，通用转发表代码)

                            → Lab4-2_enhanced/forward_raw_enhanced_shorekeeper.c
                              (Shorekeeper专用，修改了转发表配置)

                            → Lab4-2_enhanced/forward_raw_enhanced_carlotta.c
                              (Carlotta专用，修改了转发表配置)
```

### 增强版的3个转发器程序的差异

- 核心转发逻辑**完全相同**（代码复用）
- 仅**转发表配置不同**（`init_forward_table()`函数）
- 体现了**通用转发器设计**的优势

这种设计实现了**配置与逻辑分离**，是网络设备软件设计的重要原则。

---

## 九、数据包转发流程对比

### Lab4-2（单跳转发）

```
┌──────────────────────────────────────────────────┐
│ Camellya 构造数据包                               │
│ - 以太网目标MAC: Shorekeeper                      │
│ - IP目标地址: Jinhsi                              │
│ - TTL: 255                                        │
└──────────────┬───────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────────────────┐
│ Shorekeeper 接收并转发                            │
│ - 修改以太网目标MAC: Jinhsi                       │
│ - IP目标地址不变: Jinhsi                          │
│ - TTL: 255 → 254                                  │
└──────────────┬───────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────────────────┐
│ Jinhsi 接收                                       │
│ - 源IP: Camellya (保持不变)                       │
│ - TTL: 254                                        │
└──────────────────────────────────────────────────┘
```

### Lab4-2_enhanced（多跳转发）

```
┌──────────────────────────────────────────────────┐
│ Camellya 构造数据包                               │
│ - 以太网目标MAC: Phrolova (第一跳)                │
│ - IP目标地址: Jinhsi (最终目标)                   │
│ - TTL: 255                                        │
└──────────────┬───────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────────────────┐
│ Phrolova 查表转发                                 │
│ - 查找转发表: 192.168.10.102 → Shorekeeper       │
│ - 修改以太网目标MAC: Shorekeeper                  │
│ - IP目标地址不变: Jinhsi                          │
│ - TTL: 255 → 254                                  │
└──────────────┬───────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────────────────┐
│ Shorekeeper 查表转发                              │
│ - 查找转发表: 192.168.10.102 → Carlotta          │
│ - 修改以太网目标MAC: Carlotta                     │
│ - IP目标地址不变: Jinhsi                          │
│ - TTL: 254 → 253                                  │
└──────────────┬───────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────────────────┐
│ Carlotta 查表转发                                 │
│ - 查找转发表: 192.168.10.102 → Jinhsi            │
│ - 修改以太网目标MAC: Jinhsi (最后一跳)            │
│ - IP目标地址不变: Jinhsi                          │
│ - TTL: 253 → 252                                  │
└──────────────┬───────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────────────────┐
│ Jinhsi 接收                                       │
│ - 源IP: Camellya (保持不变)                       │
│ - 目标IP: Jinhsi (保持不变)                       │
│ - TTL: 252 (递减了3次)                            │
└──────────────────────────────────────────────────┘
```

---

## 十、核心概念验证

### Lab4-2 验证的概念

1. ✅ MAC地址在单跳中的更新
2. ✅ IP地址在转发中保持不变
3. ✅ TTL机制
4. ✅ 原始套接字的使用

### Lab4-2_enhanced 额外验证的概念

5. ✅ **转发表机制**：每个路由器独立查表
6. ✅ **逐跳转发**：每跳只知道下一跳，不知道完整路径
7. ✅ **多跳路由**：数据包经过多个中间节点
8. ✅ **MAC地址逐跳更新**：每跳都更新为下一跳的MAC
9. ✅ **TTL递减**：防止数据包无限循环
10. ✅ **循环检测**：避免数据包回环

---

## 十一、实现难度对比

| 维度 | Lab4-2 | Lab4-2_enhanced |
|------|--------|-----------------|
| **网络配置** | ⭐⭐ | ⭐⭐⭐⭐ |
| **代码编写** | ⭐⭐ | ⭐⭐⭐ |
| **调试难度** | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| **理解深度** | ⭐⭐ | ⭐⭐⭐⭐ |
| **实际意义** | ⭐⭐ | ⭐⭐⭐⭐⭐ |

### Lab4-2_enhanced 的调试挑战

1. **多主机协同**：需要5台主机同时运行
2. **转发表配置**：每个转发器的转发表必须正确配置
3. **MAC地址匹配**：6个MAC地址必须准确无误
4. **启动顺序**：必须按正确顺序启动（接收端→转发器3→2→1→发送端）
5. **错误定位**：出错时难以定位是哪一跳出问题

---

## 十二、总结

### Lab4-2：基础实验

- **目标**：理解原始套接字和单跳转发
- **适合**：初学者入门
- **价值**：掌握基本概念

### Lab4-2_enhanced：进阶实验

- **目标**：理解真实路由器的转发机制
- **适合**：深入学习网络原理
- **价值**：接近真实网络设备的工作方式

**Lab4-2_enhanced 更接近真实的互联网工作原理**：
- 数据包经过多个路由器
- 每个路由器独立决策（查表）
- 只修改MAC地址，不修改IP地址
- TTL防止循环转发

这是对计算机网络"网络层"和"数据链路层"协同工作的**最佳实验验证**。
