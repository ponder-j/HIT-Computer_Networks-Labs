# 计算机网络实验 - Lab2 可靠数据传输协议

本项目包含两个实验,分别实现了停等协议和GBN协议。

## 实验概览

### Lab2-1: 停等协议 (Stop-and-Wait Protocol)
- **目录**: `Lab2-1/`
- **实现内容**: 基于UDP的停等协议
- **特点**:
  - 发送窗口大小为1
  - 简单可靠
  - 效率较低但易于理解
- **详细说明**: 查看 `Lab2-1/README.md`

### Lab2-2: GBN协议 (Go-Back-N Protocol)
- **目录**: `Lab2-2/`
- **实现内容**: 基于UDP的GBN滑动窗口协议
- **特点**:
  - 发送窗口大小为10
  - 累积确认机制
  - 效率高于停等协议
- **详细说明**: 查看 `Lab2-2/README.md`

## 快速开始

### 环境要求
- Windows操作系统
- C++编译器 (MinGW/MSVC)
- Winsock2库支持

### 编译所有程序

```bash
# 进入Lab2-1目录
cd Lab2-1
g++ server.cpp -o server.exe -lws2_32
g++ client.cpp -o client.exe -lws2_32

# 进入Lab2-2目录
cd ../Lab2-2
g++ server.cpp -o server.exe -lws2_32
g++ client.cpp -o client.exe -lws2_32
```

### 运行测试

#### 测试停等协议
```bash
# 终端1
cd Lab2-1
./server.exe

# 终端2
cd Lab2-1
./client.exe
# 输入: -test 0.2
```

#### 测试GBN协议
```bash
# 终端1
cd Lab2-2
./server.exe

# 终端2
cd Lab2-2
./client.exe
# 输入: -testgbn 0.2 0.2
```

## 协议对比

| 特性         | 停等协议        | GBN协议          |
| ------------ | --------------- | ---------------- |
| 发送窗口     | 1               | 10               |
| 接收窗口     | 1               | 1                |
| 序列号空间   | 2 (0,1)         | 20 (0-19)        |
| 确认机制     | 单独确认        | 累积确认         |
| 信道利用率   | 低              | 高               |
| 重传策略     | 重传当前包      | 回退N重传多个包  |
| 实现复杂度   | 简单            | 中等             |
| 超时机制     | select()        | 非阻塞+计时器    |

## 实验要点

### 停等协议 (Lab2-1)
1. **数据帧格式**: Seq(1字节) + Data(≤1024字节) + Flag(1字节)
2. **ACK格式**: ACK(1字节)
3. **序列号**: 0和1交替使用
4. **超时重传**: 2秒超时,最多重传5次
5. **丢包模拟**: 客户端可配置丢包率

### GBN协议 (Lab2-2)
1. **数据帧格式**: Seq(1字节) + Data(≤1024字节) + Flag(1字节)
2. **ACK格式**: ACK(1字节) + Flag(1字节)
3. **序列号**: 0-19循环使用
4. **滑动窗口**: 大小为10,满足 W+1 ≤ SEQ_SIZE
5. **累积确认**: ACK=n表示n及之前的包都已收到
6. **Go-Back-N**: 超时后回退重传所有未确认的包
7. **丢包模拟**: 支持数据包丢失和ACK丢失模拟

## 文件结构

```
Lab2-1/                      # 停等协议实验
├── server.cpp               # 服务器端程序
├── client.cpp               # 客户端程序
├── README.md                # 详细说明文档
└── 实验要求.md              # 实验要求

Lab2-2/                      # GBN协议实验
├── server.cpp               # 服务器端程序
├── client.cpp               # 客户端程序
├── README.md                # 详细说明文档
└── 实验要求.md              # 实验要求

README.md                    # 本文件
```

## 实验报告要求

实验报告应包含以下内容:

1. **协议设计**
   - 数据分组格式和确认分组格式
   - 各个域的作用说明

2. **流程设计**
   - 协议两端程序流程图
   - 协议典型交互过程图

3. **测试验证**
   - 数据分组丢失验证模拟方法
   - 实验验证结果截图和说明

4. **技术实现**
   - 程序实现的主要类或函数及其作用
   - UDP编程的主要特点
   - 计时器实现方法

5. **源代码**
   - 详细注释的源程序
   - 编译和运行说明

## 关键技术

### 1. UDP Socket编程
```cpp
// 创建UDP socket
SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

// 发送数据
sendto(sock, data, len, 0, (sockaddr*)&addr, addrLen);

// 接收数据
recvfrom(sock, buffer, bufLen, 0, (sockaddr*)&addr, &addrLen);
```

### 2. 超时机制

**停等协议** - 使用select():
```cpp
fd_set readSet;
FD_ZERO(&readSet);
FD_SET(sock, &readSet);

timeval timeout;
timeout.tv_sec = 2;
timeout.tv_usec = 0;

select(0, &readSet, NULL, NULL, &timeout);
```

**GBN协议** - 非阻塞socket:
```cpp
u_long iMode = 1;  // 非阻塞
ioctlsocket(sock, FIONBIO, &iMode);

// 计时器递增,检测超时
timer++;
if (timer >= TIMEOUT_THRESHOLD) {
    timeoutHandler();
}
```

### 3. 丢包模拟
```cpp
bool simulateLoss(float lossRate) {
    float random = (float)rand() / RAND_MAX;
    return random < lossRate;
}
```

## 测试建议

### 1. 基础功能测试
- ✓ 无丢包情况下正常传输
- ✓ 序列号正确切换
- ✓ ACK正确发送和接收

### 2. 丢包测试
- ✓ 低丢包率 (10-20%): 观察重传
- ✓ 中丢包率 (30-40%): 测试可靠性
- ✓ 高丢包率 (50%+): 压力测试

### 3. 性能对比
- 比较停等协议和GBN协议的传输时间
- 观察不同丢包率下的性能差异
- 分析窗口大小对GBN效率的影响

## 常见问题

### Q: 编译时提示找不到ws2_32.lib

A: 使用 `-lws2_32` 链接库:
```bash
g++ server.cpp -o server.exe -lws2_32
```

### Q: 端口被占用怎么办?

A: 修改源代码中的 `SERVER_PORT` 宏定义,改为其他未使用的端口。

### Q: 防火墙阻止连接?

A: 在防火墙中允许程序通信,或临时关闭防火墙测试。

### Q: 丢包率设置多少合适?

A:
- 演示正常流程: 0-0.1
- 演示重传机制: 0.2-0.3
- 压力测试: 0.4-0.5

### Q: GBN为什么要重传多个包?

A: GBN的接收窗口为1,只能按序接收。丢失一个包后,后续乱序包都会被拒绝,所以需要回退重传。

## 进阶扩展

### 选做内容

1. **双向数据传输**
   - 修改协议支持双向通信
   - 实现piggybacking(捎带确认)

2. **文件传输应用**
   - 基于协议实现文件传输
   - 添加文件完整性校验

3. **SR协议**
   - 将GBN改进为选择重传(SR)协议
   - 接收窗口 > 1
   - 只重传丢失的包

4. **性能优化**
   - 动态调整超时时间
   - 快速重传机制
   - 拥塞控制

## 参考资料

- 《计算机网络(第7版)》谢希仁
- RFC 1122 - 传输层协议
- Winsock2 API文档

## 许可

本项目仅用于教学和学习目的。
