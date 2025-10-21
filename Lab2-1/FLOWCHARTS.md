# 停等协议流程图

本文档使用Mermaid语法展示停等协议的详细流程。

## 1. 服务器端流程图

```mermaid
flowchart TD
    Start([服务器启动]) --> Init[初始化Winsock]
    Init --> CreateSocket[创建UDP Socket]
    CreateSocket --> Bind[绑定端口12340]
    Bind --> WaitCmd[等待客户端命令]

    WaitCmd --> RecvCmd{接收到命令?}
    RecvCmd -->|超时| WaitCmd
    RecvCmd -->|-time| SendTime[发送服务器时间]
    RecvCmd -->|-quit| SendBye[发送Goodbye]
    RecvCmd -->|-test| StartTest[开始停等协议测试]

    SendTime --> WaitCmd
    SendBye --> WaitCmd

    StartTest --> InitSeq[初始化序列号<br/>currentSeq = 0]
    InitSeq --> PrepareData[准备测试数据<br/>5个数据包]
    PrepareData --> LoopStart{还有数据?}

    LoopStart -->|是| BuildFrame[构造数据帧<br/>Seq + Data + Flag]
    LoopStart -->|否| TestComplete[测试完成]

    BuildFrame --> SendFrame[发送数据帧<br/>sendto]
    SendFrame --> StartTimer[启动计时器<br/>timeout=2秒]
    StartTimer --> WaitACK[等待ACK<br/>select]

    WaitACK --> CheckResult{收到ACK?}
    CheckResult -->|超时| CheckAttempts{重传次数<5?}
    CheckAttempts -->|是| Retransmit[重传数据帧]
    CheckAttempts -->|否| TransferFail[传输失败]
    Retransmit --> StartTimer

    CheckResult -->|收到ACK| VerifyACK{ACK正确?}
    VerifyACK -->|否| CheckAttempts
    VerifyACK -->|是| SwitchSeq[切换序列号<br/>seq = 1 - seq]

    SwitchSeq --> LoopStart

    TestComplete --> WaitCmd
    TransferFail --> WaitCmd

    style Start fill:#90EE90
    style TestComplete fill:#90EE90
    style TransferFail fill:#FFB6C1
    style SendFrame fill:#87CEEB
    style WaitACK fill:#FFD700
```

## 2. 客户端流程图

```mermaid
flowchart TD
    Start([客户端启动]) --> Init[初始化Winsock]
    Init --> CreateSocket[创建UDP Socket]
    CreateSocket --> ConfigServer[配置服务器地址<br/>127.0.0.1:12340]
    ConfigServer --> WaitInput[等待用户输入命令]

    WaitInput --> GetInput[获取用户输入]
    GetInput --> ParseCmd{解析命令}

    ParseCmd -->|-time| SendTimeCmd[发送-time命令]
    ParseCmd -->|-quit| SendQuitCmd[发送-quit命令]
    ParseCmd -->|-test| StartTest[开始停等协议测试]
    ParseCmd -->|其他| SendEcho[发送回显请求]

    SendTimeCmd --> RecvResp1[接收响应]
    RecvResp1 --> WaitInput

    SendQuitCmd --> RecvResp2[接收响应]
    RecvResp2 --> Exit([退出程序])

    SendEcho --> RecvResp3[接收响应]
    RecvResp3 --> WaitInput

    StartTest --> ParseLoss[解析丢包率参数]
    ParseLoss --> InitExpSeq[初始化期望序列号<br/>expectedSeq = 0]
    InitExpSeq --> SendTestCmd[发送-test命令]
    SendTestCmd --> SetTimeout[设置接收超时<br/>10秒]

    SetTimeout --> RecvLoop[接收循环]
    RecvLoop --> RecvFrame{接收数据帧?}

    RecvFrame -->|超时| RecvTimeout[接收超时<br/>测试结束]
    RecvFrame -->|收到| SimulateLoss{模拟丢包?}

    SimulateLoss -->|是| DropPacket[丢弃数据包<br/>不发送ACK]
    SimulateLoss -->|否| CheckSeq{检查序列号}

    DropPacket --> RecvLoop

    CheckSeq -->|不匹配| ResendOldACK[重发上一个ACK]
    CheckSeq -->|匹配| AcceptData[接受数据]

    ResendOldACK --> RecvLoop

    AcceptData --> SendACK[发送ACK<br/>ack = expectedSeq]
    SendACK --> SwitchExpSeq[切换期望序列号<br/>expectedSeq = 1 - expectedSeq]
    SwitchExpSeq --> CheckEnd{是否最后一帧?<br/>flag == 1}

    CheckEnd -->|否| RecvLoop
    CheckEnd -->|是| TestComplete[测试完成<br/>显示统计]

    RecvTimeout --> WaitInput
    TestComplete --> WaitInput

    style Start fill:#90EE90
    style Exit fill:#90EE90
    style TestComplete fill:#90EE90
    style DropPacket fill:#FFB6C1
    style SendACK fill:#87CEEB
    style RecvFrame fill:#FFD700
```

## 3. 停等协议交互序列图

```mermaid
sequenceDiagram
    participant C as 客户端
    participant S as 服务器

    Note over C,S: 初始化阶段
    C->>C: 启动并配置
    S->>S: 启动并监听12340

    Note over C,S: 命令请求阶段
    C->>S: 发送命令 "-test"
    S->>S: 解析命令，准备数据
    S->>S: currentSeq = 0
    C->>C: expectedSeq = 0

    Note over C,S: 数据帧1传输 (Seq=0)
    S->>C: 数据帧[Seq=0, Data="第一个包", Flag=0]
    S->>S: 启动计时器(2秒)
    C->>C: 检查序列号: 0 == 0 ✓
    C->>S: ACK[0]
    S->>S: 收到ACK[0] ✓
    S->>S: currentSeq = 1
    C->>C: expectedSeq = 1

    Note over C,S: 数据帧2传输 (Seq=1, 丢包)
    S->>C: 数据帧[Seq=1, Data="第二个包", Flag=0]
    S->>S: 启动计时器(2秒)
    C->>C: ❌ 模拟丢包，丢弃数据
    Note over S: 等待ACK...
    Note over S: 超时(2秒)!
    S->>C: 【重传】数据帧[Seq=1, Data="第二个包"]
    S->>S: 重启计时器(2秒)
    C->>C: 检查序列号: 1 == 1 ✓
    C->>S: ACK[1]
    S->>S: 收到ACK[1] ✓
    S->>S: currentSeq = 0
    C->>C: expectedSeq = 0

    Note over C,S: 数据帧3传输 (Seq=0)
    S->>C: 数据帧[Seq=0, Data="第三个包", Flag=0]
    C->>S: ACK[0]
    S->>S: currentSeq = 1
    C->>C: expectedSeq = 1

    Note over C,S: 数据帧4传输 (Seq=1)
    S->>C: 数据帧[Seq=1, Data="第四个包", Flag=0]
    C->>S: ACK[1]
    S->>S: currentSeq = 0
    C->>C: expectedSeq = 0

    Note over C,S: 数据帧5传输 (Seq=0, 最后一帧)
    S->>C: 数据帧[Seq=0, Data="最后一个包", Flag=1]
    C->>C: 检查: Flag == 1 (结束标志)
    C->>S: ACK[0]
    S->>S: 收到ACK[0] ✓

    Note over C,S: 传输完成
    S->>S: ✓ 所有数据发送完成
    C->>C: ✓ 所有数据接收完成
```

## 4. 停等协议状态转换图

### 服务器状态机

```mermaid
stateDiagram-v2
    [*] --> 等待命令
    等待命令 --> 准备数据: 收到-test命令
    准备数据 --> 发送数据帧: 初始化Seq=0

    发送数据帧 --> 等待ACK: 发送并启动计时器

    等待ACK --> 检查ACK: 收到ACK
    等待ACK --> 超时处理: 超时(2秒)

    超时处理 --> 发送数据帧: attempts < 5<br/>(重传)
    超时处理 --> 传输失败: attempts >= 5

    检查ACK --> 切换序列号: ACK正确
    检查ACK --> 超时处理: ACK错误

    切换序列号 --> 发送数据帧: 还有数据
    切换序列号 --> 传输完成: 无数据

    传输完成 --> 等待命令
    传输失败 --> 等待命令

    等待命令 --> [*]: 程序退出
```

### 客户端状态机

```mermaid
stateDiagram-v2
    [*] --> 等待输入
    等待输入 --> 发送命令: 用户输入-test
    发送命令 --> 等待数据: 初始化expectedSeq=0

    等待数据 --> 检查丢包: 收到数据帧
    等待数据 --> 测试超时: 超时(10秒)

    检查丢包 --> 丢弃数据: 随机丢包
    检查丢包 --> 检查序列号: 不丢包

    丢弃数据 --> 等待数据: 不发送ACK

    检查序列号 --> 发送ACK: Seq匹配
    检查序列号 --> 重发旧ACK: Seq不匹配

    重发旧ACK --> 等待数据

    发送ACK --> 切换序列号: 发送ACK=expectedSeq
    切换序列号 --> 检查结束: expectedSeq = 1-expectedSeq

    检查结束 --> 等待数据: Flag != 1
    检查结束 --> 接收完成: Flag == 1

    接收完成 --> 等待输入
    测试超时 --> 等待输入

    等待输入 --> [*]: 退出命令
```

## 5. 数据结构图

```mermaid
classDiagram
    class DataFrame {
        +unsigned char seq
        +char data[1024]
        +unsigned char flag
        +getSeq() unsigned char
        +getData() char*
        +isLastFrame() bool
    }

    class AckFrame {
        +unsigned char ack
        +getAck() unsigned char
    }

    class StopAndWait {
        -unsigned char currentSeq
        -int maxAttempts
        -int timeoutMs
        +sendDataFrame() bool
        +receiveAck() bool
        +switchSeq() void
    }

    DataFrame --> StopAndWait : 使用
    AckFrame --> StopAndWait : 使用

    note for DataFrame "数据帧结构\nseq: 0或1\ndata: 最大1024字节\nflag: 0=普通 1=结束"

    note for AckFrame "确认帧结构\nack: 确认序列号(0或1)"

    note for StopAndWait "停等协议核心\n序列号交替\n超时重传机制"
```

## 6. 时序关系图

```mermaid
gantt
    title 停等协议传输时间线 (5个数据包)
    dateFormat X
    axisFormat %L ms

    section 数据包1(Seq=0)
    发送数据帧     :a1, 0, 10ms
    等待ACK       :a2, 10, 50ms
    收到ACK       :milestone, a3, 60

    section 数据包2(Seq=1)
    发送数据帧     :b1, 70, 10ms
    等待ACK       :b2, 80, 2000ms
    超时!         :crit, b3, 2080, 10ms
    重传数据帧     :b4, 2090, 10ms
    等待ACK       :b5, 2100, 50ms
    收到ACK       :milestone, b6, 2150

    section 数据包3(Seq=0)
    发送数据帧     :c1, 2160, 10ms
    等待ACK       :c2, 2170, 50ms
    收到ACK       :milestone, c3, 2220

    section 数据包4(Seq=1)
    发送数据帧     :d1, 2230, 10ms
    等待ACK       :d2, 2240, 50ms
    收到ACK       :milestone, d3, 2290

    section 数据包5(Seq=0)
    发送数据帧     :e1, 2300, 10ms
    等待ACK       :e2, 2310, 50ms
    收到ACK       :milestone, e3, 2360
```

## 7. 协议对比图

```mermaid
graph LR
    subgraph 停等协议
        A1[发送Seq=0] --> A2[等待ACK]
        A2 --> A3[收到ACK=0]
        A3 --> A4[发送Seq=1]
        A4 --> A5[等待ACK]
        A5 --> A6[收到ACK=1]
        A6 --> A7[发送Seq=0]
    end

    style A1 fill:#87CEEB
    style A4 fill:#87CEEB
    style A7 fill:#87CEEB
    style A2 fill:#FFD700
    style A5 fill:#FFD700
    style A3 fill:#90EE90
    style A6 fill:#90EE90
```

## 使用说明

这些流程图可以在支持Mermaid的环境中渲染，例如：
- GitHub/GitLab的Markdown文件
- VS Code (安装Mermaid插件)
- 在线编辑器: https://mermaid.live/

### 在Markdown中使用

直接将上述代码块复制到Markdown文件中，Mermaid渲染器会自动生成图表。

### 流程图说明

1. **服务器端流程图**: 展示服务器从启动到处理停等协议的完整流程
2. **客户端流程图**: 展示客户端从启动到接收数据的完整流程
3. **交互序列图**: 展示客户端和服务器之间的详细交互过程
4. **状态转换图**: 展示协议状态机的转换关系
5. **数据结构图**: 展示关键数据结构和类关系
6. **时序关系图**: 展示实际传输中的时间分布
7. **协议对比图**: 展示停等协议的基本工作模式
