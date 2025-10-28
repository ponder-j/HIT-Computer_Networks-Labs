# 实验4.1 网络拓扑图

## 基本拓扑结构

```mermaid
graph LR
    A[Camellya<br/>发送端<br/>IP: 192.168.10.100]
    B[Shorekeeper<br/>转发端<br/>IP: 192.168.10.101]
    C[Jinhsi<br/>接收端<br/>IP: 192.168.10.102]

    A -->|UDP 端口12345| B
    B -->|UDP 端口54321| C

    style A fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    style B fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style C fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
```

## 详细数据流动图

```mermaid
sequenceDiagram
    participant A as 未命名1<br/>(发送端)
    participant B as 未命名2<br/>(转发端)
    participant C as 未命名3<br/>(接收端)

    Note over C: 1. 启动接收程序<br/>监听端口54321
    Note over B: 2. 启动转发程序<br/>监听端口12345
    Note over A: 3. 运行发送程序

    A->>B: 发送UDP数据报<br/>目标端口:12345<br/>消息: "Hello, this is a UDP datagram!"
    Note over B: 接收数据<br/>修改目标地址
    B->>C: 转发UDP数据报<br/>目标端口:54321<br/>消息: "Hello, this is a UDP datagram!"
    Note over C: 接收并显示消息
```

## 网络层次结构

```mermaid
graph TB
    subgraph 物理网络
        SW[交换机/路由器]
    end

    subgraph 虚拟机1
        A1[未命名1]
        A2[send_ip.c]
    end

    subgraph 虚拟机2
        B1[未命名2]
        B2[forward_ip.c]
    end

    subgraph 虚拟机3
        C1[未命名3]
        C2[recv_ip.c]
    end

    A2 -.->|发送| A1
    A1 --> SW
    SW --> B1
    B1 -.->|接收+转发| B2
    B2 -.->|发送| B1
    B1 --> SW
    SW --> C1
    C1 -.->|接收| C2

    style A1 fill:#e1f5ff
    style B1 fill:#fff3e0
    style C1 fill:#f3e5f5
    style SW fill:#e8f5e9,stroke:#2e7d32,stroke-width:3px
```

## 端口映射关系

```mermaid
graph LR
    subgraph 未命名1_发送端
        P1[程序: send_ip<br/>发送到端口: 12345]
    end

    subgraph 未命名2_转发端
        P2[监听端口: 12345<br/>程序: forward_ip<br/>发送到端口: 54321]
    end

    subgraph 未命名3_接收端
        P3[监听端口: 54321<br/>程序: recv_ip]
    end

    P1 -->|UDP| P2
    P2 -->|UDP| P3

    style P1 fill:#bbdefb,stroke:#1976d2
    style P2 fill:#ffe0b2,stroke:#f57c00
    style P3 fill:#e1bee7,stroke:#7b1fa2
```

## 数据包结构

```mermaid
graph TD
    A[以太网帧] --> B[IP数据报]
    B --> C[UDP数据报]
    C --> D[应用数据]

    D --> E["Hello, this is a UDP datagram!"]

    style A fill:#ffebee
    style B fill:#e3f2fd
    style C fill:#e8f5e9
    style D fill:#fff3e0
    style E fill:#fce4ec
```

## 实验步骤流程图

```mermaid
flowchart TD
    Start([开始实验]) --> Check[检查三台虚拟机<br/>网络连通性]
    Check --> Modify[修改代码中的IP地址]
    Modify --> Copy[将代码复制到<br/>对应虚拟机]
    Copy --> Compile[在各主机上编译程序]
    Compile --> Firewall[配置防火墙规则]
    Firewall --> Run1[第1步: 启动未命名3<br/>接收端程序]
    Run1 --> Run2[第2步: 启动未命名2<br/>转发端程序]
    Run2 --> Run3[第3步: 运行未命名1<br/>发送端程序]
    Run3 --> Result{数据接收<br/>成功?}
    Result -->|是| Success([实验成功])
    Result -->|否| Debug[检查:<br/>1. IP地址<br/>2. 防火墙<br/>3. 网络连通]
    Debug --> Run1

    style Start fill:#c8e6c9
    style Success fill:#a5d6a7
    style Debug fill:#ffccbc
    style Result fill:#fff59d
```

---

# 实验4.3 网络拓扑图 - 基于双网口主机的路由转发

## 基本拓扑结构

```mermaid
graph TB
    subgraph 管理网络["SSH管理网络 (192.168.10.0/24)"]
        M1[Camellya<br/>192.168.10.100<br/>ens33]
        M2[Shorekeeper<br/>192.168.10.101<br/>ens33]
        M3[Jinhsi<br/>192.168.10.102<br/>ens33]
    end

    subgraph 实验网络A["实验网段A (192.168.1.0/24)"]
        A1[Camellya<br/>源主机<br/>192.168.1.2<br/>ens34]
        A2[Shorekeeper<br/>路由器eth0<br/>192.168.1.1<br/>ens34]
    end

    subgraph 实验网络B["实验网段B (192.168.2.0/24)"]
        B1[Shorekeeper<br/>路由器eth1<br/>192.168.2.1<br/>ens37]
        B2[Jinhsi<br/>目的主机<br/>192.168.2.2<br/>ens34]
    end

    A1 <-->|UDP数据| A2
    A2 <-->|路由转发| B1
    B1 <-->|UDP数据| B2

    style A1 fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    style A2 fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style B1 fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style B2 fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    style 管理网络 fill:#e8f5e9,stroke:#2e7d32,stroke-width:1px,stroke-dasharray: 5 5
    style 实验网络A fill:#fff8e1,stroke:#f57f17,stroke-width:2px
    style 实验网络B fill:#fce4ec,stroke:#c2185b,stroke-width:2px
```

## 详细双网卡路由拓扑

```mermaid
graph LR
    subgraph 网段A["网段A: 192.168.1.0/24"]
        Source[源主机 Camellya<br/>IP: 192.168.1.2/24<br/>网卡: ens37<br/>网关: 192.168.1.1]
        RouterA[路由器接口A<br/>IP: 192.168.1.1/24<br/>网卡: ens37]
    end
    subgraph Router["路由主机 Shorekeeper"]
        RouterA -.->|IP转发| RouterB
        ForwardEngine[路由转发引擎<br/>查找路由表<br/>修改MAC地址<br/>TTL递减<br/>重算校验和]
    end
    subgraph 网段B["网段B: 192.168.2.0/24"]
        RouterB[路由器接口B<br/>IP: 192.168.2.1/24<br/>网卡: ens38]
        Dest[目的主机 Jinhsi<br/>IP: 192.168.2.2/24<br/>网卡: ens37<br/>网关: 192.168.2.1]
    end
    Source -->|1\. 发送数据包<br/>目标IP: 192.168.2.2| RouterA
    RouterA -->|2\. 接收| ForwardEngine
    ForwardEngine -->|3\. 转发| RouterB
    RouterB -->|4\. 发送到目的主机| Dest
    style Source fill:#bbdefb,stroke:#1976d2,stroke-width:2px
    style RouterA fill:#ffe0b2,stroke:#f57c00,stroke-width:2px
    style ForwardEngine fill:#fff9c4,stroke:#f9a825,stroke-width:2px
    style RouterB fill:#ffe0b2,stroke:#f57c00,stroke-width:2px
    style Dest fill:#e1bee7,stroke:#7b1fa2,stroke-width:2px
    style 网段A fill:#e3f2fd,stroke:#1565c0,stroke-width:2px
    style 网段B fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px
    style Router fill:#fff3e0,stroke:#e65100,stroke-width:3px
```

## 数据包转发流程

```mermaid
sequenceDiagram
    participant C as Camellya<br/>192.168.1.2
    participant RA as Shorekeeper-ens34<br/>192.168.1.1
    participant RB as Shorekeeper-ens37<br/>192.168.2.1
    participant J as Jinhsi<br/>192.168.2.2

    Note over C: 发送端准备数据
    Note over J: 1. 启动接收程序<br/>监听端口12345
    Note over RA,RB: 2. 启动路由程序<br/>监听两个接口
    Note over C: 3. 发送UDP数据包

    C->>RA: UDP包 (目标IP: 192.168.2.2:12345)<br/>源MAC: Camellya MAC<br/>目标MAC: Shorekeeper-ens34 MAC<br/>TTL: 64

    Note over RA: 接收数据包<br/>from ens34
    Note over RA,RB: 路由决策:<br/>1. 查找路由表<br/>2. 确定出接口: ens37<br/>3. 修改MAC地址<br/>4. TTL递减 (64→63)<br/>5. 重算IP校验和

    RB->>J: UDP包 (目标IP: 192.168.2.2:12345)<br/>源MAC: Shorekeeper-ens37 MAC<br/>目标MAC: Jinhsi MAC<br/>TTL: 63

    Note over J: 接收并显示消息<br/>来源: 192.168.1.2

    rect rgb(200, 230, 201)
        Note over C,J: IP地址始终保持不变:<br/>源IP: 192.168.1.2<br/>目标IP: 192.168.2.2
    end

    rect rgb(255, 224, 178)
        Note over C,J: MAC地址每跳都变化:<br/>第1跳: Camellya → Shorekeeper-ens34<br/>第2跳: Shorekeeper-ens37 → Jinhsi
    end
```

## 网卡配置详细图

```mermaid
graph TD
    subgraph Camellya["Camellya (源主机)"]
        C1[ens33: 192.168.10.100/24<br/>桥接网卡 - SSH管理用]
        C2[ens34: 192.168.1.2/24<br/>实验网卡<br/>网关: 192.168.1.1]
    end

    subgraph Shorekeeper["Shorekeeper (路由器)"]
        S1[ens33: 192.168.10.101/24<br/>桥接网卡 - SSH管理用]
        S2[ens34: 192.168.1.1/24<br/>连接网段A]
        S3[ens37: 192.168.2.1/24<br/>连接网段B]
        FWD[IP转发: 已启用<br/>net.ipv4.ip_forward=1]
    end

    subgraph Jinhsi["Jinhsi (目的主机)"]
        J1[ens33: 192.168.10.102/24<br/>桥接网卡 - SSH管理用]
        J2[ens34: 192.168.2.2/24<br/>实验网卡<br/>网关: 192.168.2.1]
    end

    C2 <-->|网段A| S2
    S3 <-->|网段B| J2
    S2 -.->|路由| FWD
    FWD -.->|转发| S3

    style C1 fill:#e8f5e9,stroke:#2e7d32
    style C2 fill:#e1f5ff,stroke:#01579b,stroke-width:3px
    style S1 fill:#e8f5e9,stroke:#2e7d32
    style S2 fill:#fff3e0,stroke:#e65100,stroke-width:3px
    style S3 fill:#fff3e0,stroke:#e65100,stroke-width:3px
    style FWD fill:#fff9c4,stroke:#f9a825,stroke-width:2px
    style J1 fill:#e8f5e9,stroke:#2e7d32
    style J2 fill:#f3e5f5,stroke:#4a148c,stroke-width:3px
```

## 路由表示意图

```mermaid
graph TB
    subgraph RouterTable["Shorekeeper 路由表"]
        RT[路由表查找算法]
        R1["条目1:<br/>目标: 192.168.1.0/24<br/>接口: ens34<br/>类型: 直接连接"]
        R2["条目2:<br/>目标: 192.168.2.0/24<br/>接口: ens37<br/>类型: 直接连接"]
        R3["条目3:<br/>目标: 0.0.0.0/0<br/>网关: 192.168.10.1<br/>接口: ens33<br/>类型: 默认路由"]
    end

    PKT[收到数据包<br/>目标IP: 192.168.2.2]

    PKT --> RT
    RT --> R1
    RT --> R2
    RT --> R3
    R2 --> OUT[匹配成功!<br/>从 ens37 转发]

    style PKT fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    style RT fill:#fff9c4,stroke:#f9a825,stroke-width:2px
    style R1 fill:#f5f5f5,stroke:#9e9e9e
    style R2 fill:#c8e6c9,stroke:#2e7d32,stroke-width:3px
    style R3 fill:#f5f5f5,stroke:#9e9e9e
    style OUT fill:#a5d6a7,stroke:#1b5e20,stroke-width:2px
```

## MAC地址和IP地址变化对比

```mermaid
graph LR
    subgraph Stage1["阶段1: Camellya发送"]
        P1["源MAC: 00:0c:29:XX:XX:XX<br/>目标MAC: 00:0c:29:YY:YY:YY<br/>源IP: 192.168.1.2<br/>目标IP: 192.168.2.2<br/>TTL: 64"]
    end

    subgraph Stage2["阶段2: Shorekeeper-ens34接收"]
        P2["源MAC: 00:0c:29:XX:XX:XX<br/>目标MAC: 00:0c:29:YY:YY:YY<br/>源IP: 192.168.1.2<br/>目标IP: 192.168.2.2<br/>TTL: 64"]
    end

    subgraph Stage3["阶段3: Shorekeeper-ens37转发"]
        P3["源MAC: 00:0c:29:YY:YY:ZZ<br/>目标MAC: 00:0c:29:ZZ:ZZ:ZZ<br/>源IP: 192.168.1.2<br/>目标IP: 192.168.2.2<br/>TTL: 63"]
    end

    subgraph Stage4["阶段4: Jinhsi接收"]
        P4["源MAC: 00:0c:29:YY:YY:ZZ<br/>目标MAC: 00:0c:29:ZZ:ZZ:ZZ<br/>源IP: 192.168.1.2<br/>目标IP: 192.168.2.2<br/>TTL: 63"]
    end

    P1 --> P2 --> P3 --> P4

    style P1 fill:#bbdefb,stroke:#1976d2
    style P2 fill:#ffe0b2,stroke:#f57c00
    style P3 fill:#ffe0b2,stroke:#e65100
    style P4 fill:#e1bee7,stroke:#7b1fa2
```

## 实验配置步骤流程

```mermaid
flowchart TD
    Start([开始实验4.3配置]) --> Shutdown[关闭所有虚拟机]

    Shutdown --> AddNIC1[在虚拟机管理器中:<br/>Camellya添加1张网卡<br/>连接到intnet-A]
    AddNIC1 --> AddNIC2[Shorekeeper添加2张网卡<br/>网卡2: intnet-A<br/>网卡3: intnet-B]
    AddNIC2 --> AddNIC3[Jinhsi添加1张网卡<br/>连接到intnet-B]

    AddNIC3 --> StartVM[启动所有虚拟机]
    StartVM --> CheckNIC[用 ip a 查看新网卡名称<br/>记录: ens34, ens37等]

    CheckNIC --> ConfigC[配置Camellya:<br/>ens34 = 192.168.1.2/24<br/>网关 = 192.168.1.1]
    ConfigC --> ConfigS[配置Shorekeeper:<br/>ens34 = 192.168.1.1/24<br/>ens37 = 192.168.2.1/24<br/>启用IP转发]
    ConfigS --> ConfigJ[配置Jinhsi:<br/>ens34 = 192.168.2.2/24<br/>网关 = 192.168.2.1]

    ConfigJ --> TestPing[测试连通性:<br/>ping各网关和对端]
    TestPing --> PingOK{Ping通?}

    PingOK -->|否| Debug[检查:<br/>1. 网卡配置<br/>2. IP地址<br/>3. 路由表<br/>4. IP转发]
    Debug --> ConfigC

    PingOK -->|是| RunRecv[启动Jinhsi接收程序]
    RunRecv --> RunRoute[启动Shorekeeper路由程序<br/>sudo ./forward_dual]
    RunRoute --> RunSend[启动Camellya发送程序<br/>./send_dual]

    RunSend --> Success([实验成功!])

    style Start fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
    style Success fill:#a5d6a7,stroke:#1b5e20,stroke-width:3px
    style Debug fill:#ffccbc,stroke:#d84315
    style PingOK fill:#fff59d,stroke:#f57f00
    style ConfigS fill:#ffe0b2,stroke:#e65100,stroke-width:2px
```
