# 实验指导书：IP数据报的转发及收发

## 目录

1. [实验目的](#实验目的)
2. [实验环境](#实验环境)
3. [实验内容](#实验内容)
4. [实验步骤](#实验步骤)
5. [实验方式](#实验方式)
6. [实验报告](#实验报告)
7. [附录：原始套接字介绍](#附录原始套接字介绍)

---

## 1. 实验目的

- 了解原始套接字的基本概念和使用方法
- 掌握路由器进行IP数据报转发的基本原理
- 实现基于原始套接字的IP数据报的发送和接收
- 实现基于原始套接字的IP数据报转发,包括AF_INET和AF_PACKET原始套接字的应用

## 2. 实验环境

- **操作系统**: Linux
- **编程语言**: C
- **工具**: GCC编译器、tcpdump、Wireshark

## 3. 实验内容

### 3.1 使用虚拟机实现多主机间的UDP数据报收发及转发

利用虚拟机搭建实验环境,掌握Linux下的Socket网络编程。

**选做1**: 改进程序,示例程序只实现了一个数据包(携带1条消息)的发、转、收过程,要求实现每条消息由控制台输入,并且不限制发送消息的数目。

### 3.2 基于单网口主机的IP数据转发及收发

在局域网中,模拟IP数据报的路由转发过程。通过原始套接字实现了完整的数据封装过程,实现了UDP头部、IP头部、MAC帧头部的构造。

**选做2**: 扩展实验的网络规模,由原始方案中3台主机增加到不少于5台主机,共同完成IP数据报转发及收发过程,要求采用转发表改进示例程序,增加程序通用性。

### 3.3 基于双网口主机的路由转发

构造了静态路由表,并实现了不同子网间的IP数据报查表转发过程。

**选做3**: 通过完善路由表,改进示例程序实现双向传输。

### 3.4 路由器进行IP数据报转发的基本原理

**路由器的工作原理**:
- 路由器通过查找路由表来决定数据包的转发路径
- 路由表包含网络地址、子网掩码、网关地址和网络接口等信息
- 数据包到达路由器时,路由器解析IP头部,查找路由表,确定下一跳地址,并将数据包转发到相应的网络接口

### 3.5 示例程序说明

#### 捕获数据报

创建原始套接字:

```c
int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
if (sockfd < 0) {
    perror("Socket creation failed");
    return 1;
}
```

使用`recvfrom()`函数从网络接口捕获数据报:

```c
struct sockaddr saddr;
unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);
int data_size = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, &saddr, 
                         (socklen_t *)sizeof(saddr));
if (data_size < 0) {
    perror("Recvfrom error");
    return 1;
}
```

#### 查找路由表

解析IP头部:

```c
struct iphdr *ip_header = (struct iphdr *)(buffer + sizeof(struct ethhdr));
```

路由表结构:

```c
struct route_entry {
    uint32_t dest;
    uint32_t gateway;      // 下一跳
    uint32_t netmask;
    char interface[IFNAMSIZ];  // 接口
};
```

查找路由表函数:

```c
struct route_entry *lookup_route(uint32_t dest_ip) {
    for (int i = 0; i < route_table_size; i++) {
        if ((dest_ip & route_table[i].netmask) == 
            (route_table[i].dest & route_table[i].netmask)) {
            return &route_table[i];
        }
    }
    return NULL; // 未找到匹配的路由
}
```

#### 转发数据报

修改TTL并重新计算校验和:

```c
// 修改TTL
ip_header->ttl -= 1;
ip_header->check = 0;
ip_header->check = checksum((unsigned short *)ip_header, ip_header->ihl * 4);
```

获取网卡接口信息并发送:

```c
struct ifreq ifr, ifr_mac;
struct sockaddr_ll dest;

// 获取网卡接口索引
memset(&ifr, 0, sizeof(ifr));
snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), route->interface);
if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
    perror("ioctl");
    return 1;
}

// 构造新的以太网帧头
memcpy(eth_header->h_dest, target_mac, ETH_ALEN);
memcpy(eth_header->h_source, ifr_mac.ifr_hwaddr.sa_data, ETH_ALEN);
eth_header->h_proto = htons(ETH_P_IP);

if (sendto(sockfd, buffer, data_size, 0, (struct sockaddr *)&dest, 
           sizeof(dest)) < 0) {
    perror("Sendto error");
    return 1;
}
```

---

## 4. 实验步骤

### 4.1 使用虚拟机实现多主机间的UDP数据报收发及转发

采用UDP原始套接字,将UDP数据报由A主机发送给B主机,再由B主机转发给C主机,完成了一条消息的传送。

**注意**: 此实现方案中,UDP数据报的传输过程中目的IP地址发生了变化,因此属于代理转发的场景。

#### 4.1.1 实验环境搭建

使用虚拟机软件(如VirtualBox或VMware)在Windows笔记本电脑上创建Linux虚拟机:

1. **安装虚拟机软件**: 下载并安装VirtualBox或VMware Workstation Player
2. **下载Linux发行版**: 选择适合的Linux发行版(如Ubuntu或Debian),并下载ISO文件
3. **创建虚拟机**: 在虚拟机软件中创建新虚拟机,并指定下载的Linux ISO文件作为引导盘
4. **配置网络**: 为虚拟机配置网络设置,选择"桥接模式"(Bridged Adapter),确保虚拟机和物理机都连接到同一个局域网
5. **设置路由转发**(可选): 在虚拟机中安装和配置路由软件(如iptables或firewall)
6. **部署实验环境**: 在Linux虚拟机中安装所需的网络编程实验软件和工具

**验证网络连接**:

1. 获取IP地址:
   - 在Windows物理机上使用`ipconfig`查看物理机的IP地址
   - 在虚拟机中使用`ip a`查看虚拟机的IP地址

2. 测试连接:
   - 在虚拟机中测试连接到物理机: `ping 物理机的IP地址`
   - 在物理机中测试连接到虚拟机: `ping 虚拟机的IP地址`

通过虚拟机克隆可以得到三台Linux虚拟机,这三台虚拟机与物理主机处于同一子网中。

#### 4.1.2 示例程序

**1. 准备工作**

- 安装必要的软件包: `sudo apt-get install gcc`
- 确认网络配置正常,确保主机间能正常通信
- 确保Linux主机防火墙没有阻止UDP流量

**2. 发送IP数据报**

创建文件`send_ip.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sockfd;
    struct sockaddr_in dest_addr;
    char *message = "Hello, this is a UDP datagram!";
    int port = 12345;  // 目标端口号

    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 目标地址
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr("目标IP地址");

    // 发送数据报
    if (sendto(sockfd, message, strlen(message), 0, 
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("sendto");
        return 1;
    }

    printf("Datagram sent.\n");
    close(sockfd);
    free(buffer);
    return 0;
}
```

**3. 接收主机程序(目的主机)**

该程序接收数据包并打印内容:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 12345

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];

    // 创建UDP套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 绑定套接字到端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // 接收数据包
    int recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                           (struct sockaddr *)&client_addr, &addr_len);
    if (recv_len < 0) {
        perror("Recvfrom failed");
        return 1;
    }

    buffer[recv_len] = '\0';
    printf("Received message: %s\n", buffer);

    close(sockfd);
    return 0;
}
```

**运行步骤** (注意三个程序的启动顺序):

1. 在目的主机上运行接收程序
2. 在路由器上运行转发程序
3. 在源主机上运行发送程序

---

### 4.3 基于双网口主机的路由转发

#### 4.3.1 实验环境

**准备设备和软件**:

- **三台计算机**: 一台作为源主机,一台作为路由器,一台作为目的主机
- **网络交换机**: 用于连接这些计算机
- **操作系统**: 建议使用Linux(如Debian)
- **网络工具**: 如Wireshark(Tshark)用于捕获和分析网络数据包

**网络拓扑配置**:

1. **连接设备**: 将源主机、路由主机和目的主机通过交换机互相连接
2. **配置IP地址**:
   - 源主机: 192.168.1.2/24
   - 路由主机(两个接口):
     - 接口1(连接源主机): 192.168.1.1/24
     - 接口2(连接目的主机): 192.168.2.1/24
   - 目的主机: 192.168.2.2/24

**配置路由表**:

在源主机和目的主机上配置静态路由:

- 源主机:
  ```bash
  sudo ip route add 192.168.2.0/24 via 192.168.1.1
  ```

- 路由器: 已有两个接口连接源主机和目的主机,不需要额外配置

- 目的主机:
  ```bash
  sudo ip route add 192.168.1.0/24 via 192.168.2.1
  ```

**测试网络连接**:

1. 在源主机上发送数据包:
   ```bash
   ping 192.168.2.2
   ```

2. 使用Wireshark在路由器上捕获数据包,验证数据包是否被正确转发

**验证转发过程**:

1. 检查源主机: 确认源主机发送的数据包到达路由器
2. 检查路由器: 确认路由器捕获数据包,修改TTL并转发到目的主机
3. 检查目的主机: 确认目的主机接收到数据包并返回响应

#### 4.3.2 基于虚拟机的实验环境搭建

**配置双网口虚拟机步骤**:

1. **创建虚拟机**: 安装虚拟机软件(如VMware、VirtualBox)并创建新虚拟机
2. **添加第二个网口**: 在虚拟机设置中添加新的网络接口
3. **配置网络**: 为新网口配置IP地址、子网掩码、网关等
4. **启动虚拟机**: 检查新网口是否正常工作(使用`ipconfig`或`ifconfig`)
5. **测试连接**: 确保虚拟机可以通过新网口访问网络资源

**注意**: 双网口主机的两个网络接口通常分别属于不同的子网。如果两个网络接口都接入一个网络,要注意配置避免网络冲突。

**详细配置步骤**:

1. **配置物理主机**: 确保所有物理主机(笔记本电脑)连接到同一个Wi-Fi网络

2. **安装虚拟机软件**: 在每台笔记本电脑上安装VirtualBox或VMware,创建Linux虚拟机

3. **配置虚拟机网络**: 将虚拟机的网络适配器设置为桥接模式(Bridged Adapter)

4. **设置双网口虚拟机**:
   - 网络适配器1(eth0): 桥接模式,连接到192.168.1.0/24网段
   - 网络适配器2(eth1): 桥接模式,连接到192.168.2.0/24网段

**配置网络接口**:

编辑网络接口配置:
```bash
sudo nano /etc/network/interfaces
```

添加以下配置:
```
auto eth0
iface eth0 inet static
    address 192.168.1.1
    netmask 255.255.255.0

auto eth1
iface eth1 inet static
    address 192.168.2.1
    netmask 255.255.255.0
```

**配置客户端主机**:

客户端主机1 (192.168.1.0/24网段):
```bash
sudo route add default gw 192.168.1.1
```

客户端主机2 (192.168.2.0/24网段):
```bash
sudo route add default gw 192.168.2.1
```

#### 4.3.3 三台主机的程序示例

**1. 发送主机程序(源主机)**

发送简单的UDP数据包到路由器:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEST_IP "192.168.2.2"
#define DEST_PORT 12345
#define MESSAGE "Hello, this is a test message."

int main() {
    int sockfd;
    struct sockaddr_in dest_addr;

    // 创建UDP套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 设置目的地址
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest_addr.sin_addr);

    // 发送数据包
    if (sendto(sockfd, MESSAGE, strlen(MESSAGE), 0, 
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("Sendto failed");
        return 1;
    }

    printf("Message sent to %s:%d\n", DEST_IP, DEST_PORT);
    close(sockfd);
    return 0;
}
```

**2. 路由转发程序(中间主机)**

捕获数据包,修改TTL并转发到目的主机:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>

#define BUFFER_SIZE 65536

struct route_entry {
    uint32_t dest;
    uint32_t gateway;
    uint32_t netmask;
    char interface[IFNAMSIZ];
};

struct route_entry route_table[] = {
    {inet_addr("192.168.2.0"), inet_addr("192.168.2.1"), 
     inet_addr("255.255.255.0"), "eth1"}
};

int route_table_size = sizeof(route_table) / sizeof(route_table[0]);

unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum;
    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

struct route_entry *lookup_route(uint32_t dest_ip) {
    for (int i = 0; i < route_table_size; i++) {
        if ((dest_ip & route_table[i].netmask) == 
            (route_table[i].dest & route_table[i].netmask)) {
            return &route_table[i];
        }
    }
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr saddr;
    unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    while (1) {
        int data_size = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, &saddr, 
                                (socklen_t *)sizeof(saddr));
        if (data_size < 0) {
            perror("Recvfrom error");
            return 1;
        }

        struct ethhdr *eth_header = (struct ethhdr *)buffer;
        struct iphdr *ip_header = (struct iphdr *)(buffer + sizeof(struct ethhdr));

        printf("Captured packet from %s to %s\n", 
               inet_ntoa(*(struct in_addr *)&ip_header->saddr), 
               inet_ntoa(*(struct in_addr *)&ip_header->daddr));

        struct route_entry *route = lookup_route(ip_header->daddr);
        if (route == NULL) {
            fprintf(stderr, "No route to host\n");
            continue;
        }

        // 修改TTL
        ip_header->ttl -= 1;
        ip_header->check = 0;
        ip_header->check = checksum((unsigned short *)ip_header, ip_header->ihl * 4);

        // 发送数据包到目的主机
        struct ifreq ifr, ifr_mac;
        struct sockaddr_ll dest;

        // 获取网卡接口索引
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), route->interface);
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            perror("ioctl");
            return 1;
        }

        // 获取网卡接口MAC地址
        memset(&ifr_mac, 0, sizeof(ifr_mac));
        snprintf(ifr_mac.ifr_name, sizeof(ifr_mac.ifr_name), route->interface);
        if (ioctl(sockfd, SIOCGIFHWADDR, &ifr_mac) < 0) {
            perror("ioctl");
            return 1;
        }

        // 设置目标MAC地址
        unsigned char target_mac[ETH_ALEN] = {0x00, 0x0c, 0x29, 0x48, 0xd3, 0xf7};
        memset(&dest, 0, sizeof(dest));
        dest.sll_ifindex = ifr.ifr_ifindex;
        dest.sll_halen = ETH_ALEN;
        memcpy(dest.sll_addr, target_mac, ETH_ALEN);

        // 构造新的以太网帧头
        memcpy(eth_header->h_dest, target_mac, ETH_ALEN);
        memcpy(eth_header->h_source, ifr_mac.ifr_hwaddr.sa_data, ETH_ALEN);
        eth_header->h_proto = htons(ETH_P_IP);

        printf("Interface name: %s, index: %d\n", ifr.ifr_name, ifr.ifr_ifindex);

        if (sendto(sockfd, buffer, data_size, 0, (struct sockaddr *)&dest, 
                  sizeof(dest)) < 0) {
            perror("Sendto error");
            return 1;
        }
    }

    close(sockfd);
    free(buffer);
    return 0;
}
```

**3. 接收主机程序(目的主机)**

接收数据包并打印内容:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 12345

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];

    // 创建UDP套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 绑定套接字到端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // 接收数据包
    int recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                           (struct sockaddr *)&client_addr, &addr_len);
    if (recv_len < 0) {
        perror("Recvfrom failed");
        return 1;
    }

    buffer[recv_len] = '\0';
    printf("Received message: %s\n", buffer);

    close(sockfd);
    return 0;
}
```

**运行步骤** (注意执行顺序):

1. 在目的主机上运行接收程序
2. 在路由器上运行转发程序
3. 在源主机上运行发送程序

源主机发送的数据包将通过路由器转发到目的主机,目的主机将接收到并打印数据包的内容。

---

## 5. 实验方式

每位同学独立上机实验,记录并分析每个步骤中的程序输出,思考数据报在网络中传输的路径和延迟。

---

## 6. 实验报告

实验报告应包括以下内容:

### (1) 实验目的
简要描述本次实验的目的。

### (2) 实验环境
详细描述实验环境,包括:
- 操作系统版本
- 编程语言和编译器版本
- 使用的工具等

### (3) 实验内容
逐项描述实验内容的实现过程,包括:
- 代码实现
- 运行结果
- 遇到的问题及解决方法

### (4) 实验结果分析
对实验结果进行分析,包括:
- 数据包转发的成功率
- 性能和准确性

### (5) 实验总结
总结本次实验的收获和体会,包括:
- 对原始套接字和路由器转发机制的理解和掌握
- 实验过程中遇到的主要问题和解决方法
- 对网络编程的进一步认识和思考

### (6) 参考文献
列出在实验过程中参考的文献和资料,包括:
- 书籍
- 论文
- 在线教程等

---

## 附录. 原始套接字介绍

### 1. 概述

**原始套接字(Raw Socket)** 是一种特殊的套接字类型,也是一种强大的网络编程工具,允许应用程序直接访问网络层(IP层)数据包或链路层(数据链路层)的数据包,绕过操作系统提供的传输层接口。它不同于标准的TCP或UDP套接字,能够处理和发送自定义的网络协议数据包。这种套接字通常用于实现新的协议或对现有协议进行低级别的操作。

### 2. 定义与用途

**定义**: 原始套接字是直接基于网络层(如IP)的。当使用原始套接字发送数据时,应用程序负责构建完整的协议头。

**用途**: 原始套接字常用于构造和发送自定义的IP包,如在ping、traceroute等工具中,它们使用ICMP协议构建消息。

### 3. 创建原始套接字

创建原始套接字的函数是`socket()`,原型如下:

```c
int socket(int domain, int type, int protocol);
```

**type参数**指定套接字类型:

- `SOCK_RAW`: 原始套接字,允许直接访问底层协议(如IP、ICMP、TCP、UDP等)
- `SOCK_STREAM`: 流套接字,提供面向连接的、可靠的字节流服务(如TCP)
- `SOCK_DGRAM`: 数据报套接字,提供无连接的、不可靠的数据报服务(如UDP)

当指定type为`SOCK_RAW`时,创建的即为原始套接字。

### 4. domain参数

domain参数指定套接字使用的协议族:

#### AF_INET (IPv4协议族)
用于处理IPv4地址,创建处理IPv4数据包的原始套接字。

```c
int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

#### AF_INET6 (IPv6协议族)
用于处理IPv6地址,创建处理IPv6数据包的原始套接字。

```c
int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
```

#### AF_PACKET (数据链路层协议族)
用于直接访问网络接口层的数据包(如以太网帧),可捕获和发送链路层数据包。

```c
int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
```

#### AF_UNIX (本地通信协议族)
用于同一主机上的进程间通信(不常见)。

```c
int sockfd = socket(AF_UNIX, SOCK_RAW, 0);
```

### 5. protocol参数

protocol参数指定使用的具体协议:

- `IPPROTO_ICMP`: ICMP协议,用于实现ping工具
- `IPPROTO_TCP`: TCP协议,提供可靠的、面向连接的服务
- `IPPROTO_UDP`: UDP协议,提供无连接的、不可靠的服务
- `IPPROTO_RAW`: 原始IP协议,允许应用程序自己构建IP头部
- `IPPROTO_ICMPV6`: ICMPv6协议,用于IPv6的ping工具

**示例代码** - 创建ICMP原始套接字:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("ICMP raw socket created successfully.\n");
    close(sockfd);
    return 0;
}
```

### 6. 特权需求

由于原始套接字允许直接访问底层协议,并可能被用于伪造数据包,所以它通常需要特殊权限(如root权限)。

### 7. 工作方式

原始套接字发送数据时,需要提供完整的传输层头部(如TCP、UDP或ICMP头部),甚至是IP协议的头部。这给了我们控制头部字段的能力,例如伪造源IP地址。当使用原始套接字接收数据时,会得到底层协议的完整头部。

#### 数据包接收过程

当网络接口接收到一个数据包时,处理流程如下:

1. **网卡驱动**: 网卡驱动首先接收到数据包,并将其传递给内核的网络子系统
2. **软中断处理**: 在软中断上下文中,数据包被传递给`netif_receive_skb()`函数进行处理
3. **协议类型匹配**: 系统会检查数据包的协议类型,并匹配是否有注册的原始套接字。如果匹配成功,数据包会被克隆并交给相应的原始套接字处理
4. **协议栈处理**: 即使数据包被克隆并交给原始套接字,原始数据包仍会继续在协议栈中进行后续处理

#### 数据包发送过程

通过原始套接字发送数据包的过程:

1. **应用层构造数据包**: 应用程序通过原始套接字构造并发送数据包,包括自定义的IP头部和数据部分
2. **套接字层处理**: 数据包通过`inet_sendmsg()`和`raw_sendmsg()`函数进行处理
3. **路由和邻居子系统**: 数据包经过路由和邻居子系统的处理,确定发送路径和下一跳
4. **网卡驱动发送**: 最终,数据包被传递给网卡驱动进行实际的发送操作

#### 设置IP_HDRINCL选项

通过设置`IP_HDRINCL`选项,应用程序可以完全控制IP头部的内容,包括源IP地址、目的IP地址、协议类型和校验和等字段。这对于需要自定义IP头部的高级网络编程任务非常重要。

### 8. 用途与限制

#### 用途
原始套接字经常被用于:
- 网络诊断工具(如ping和traceroute)
- 网络攻击和防御
- 某些类型的网络测试

#### 限制
大多数操作系统默认会处理某些协议,这可能会导致原始套接字不能接收到这些协议的数据包。例如,操作系统可能会自动处理ICMP回显请求和回显应答,这意味着原始套接字可能无法看到这些数据包。

### 9. 注意事项

- **错误使用**: 由于原始套接字跳过了常规的协议处理,错误的使用可能导致不可预期的网络行为
- **操作系统保护**: 操作系统可能提供了某种形式的保护,以防止滥用原始套接字,例如对其使用进行限制

### 10. 跨平台差异

不同的操作系统可能对原始套接字的实现和行为有所不同。例如,Windows和Linux在处理和访问原始套接字时存在细微差别。

### 11. 数据的分流过程

当数据包到达主机进入系统协议栈时,协议栈会根据数据包的协议类型和目的地址将其分流到相应的协议实体,包括原始套接字。具体来说,数据包会被复制一份并传递给匹配的原始套接字。

详细过程如下:

1. **数据包到达网络接口**: 数据包首先到达网络接口(如以太网卡),并被传递到内核的网络协议栈

2. **链路层处理**: 在链路层,数据包的以太网帧头部被解析,确定数据包的类型(如IPv4、ARP等)
   - 如果存在`AF_PACKET`原始套接字,系统会检查数据包的链路层协议类型(如`ETH_P_ALL`、`ETH_P_IP`等)
   - 如果匹配成功,数据包会被克隆一份并传递给相应的`AF_PACKET`原始套接字
   - 处理过程: 数据包被传递给`packet_rcv()`函数进行处理

3. **网络层处理**: 在网络层,数据包的IP头部被解析,确定数据包的协议类型(如ICMP、TCP、UDP等)和目的地址
   - 如果存在`AF_INET`原始套接字,系统会检查数据包的网络层协议类型
   - 如果匹配成功,数据包会被克隆一份并传递给相应的`AF_INET`原始套接字
   - 处理过程: 数据包被传递给`ip_local_deliver_finish()`函数进行处理

4. **传输层处理**: 在传输层,数据包的传输层头部(如TCP、UDP头部)被解析,确定数据包的源端口和目的端口

5. **标准套接字处理**: 如果数据包的协议类型和端口号匹配某个标准套接字(如TCP或UDP套接字),数据包会被传递到该套接字

6. **BPF过滤器处理**: 如果原始套接字附加了BPF过滤器,数据包会先经过BPF过滤器的检查,只有通过过滤器的数据包才会被传递到原始套接字

### 12. 示例代码

#### AF_PACKET原始套接字

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

int main() {
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    char buffer[2048];
    while (1) {
        int len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (len > 0) {
            struct iphdr *ip = (struct iphdr *)buffer;
            struct icmphdr *icmp = (struct icmphdr *)(buffer + (ip->ihl << 2));
            printf("Received ICMP packet of length %d\n", len);
        }
    }

    close(sockfd);
    return 0;
}
```

### 13. 总结

当数据包到达链路层时,如果存在`AF_PACKET`原始套接字,数据包会被克隆并传递给匹配的`AF_PACKET`套接字进行处理。同样地,当数据包到达网络层时,如果存在`AF_INET`原始套接字,数据包会被克隆并传递给匹配的`AF_INET`套接字进行处理。这种机制确保了原始套接字能够接收到所有匹配的数据包,同时数据包仍会继续在协议栈中进行后续处理。

---

## 编译和运行说明

### 编译命令

对于所有C程序,使用以下命令编译:

```bash
gcc -o program_name program_name.c
```

### 运行权限

原始套接字程序需要root权限运行:

```bash
sudo ./program_name
```

### 常见问题

1. **权限不足**: 如果出现"Operation not permitted"错误,确保使用sudo运行程序

2. **网络接口名称**: 示例程序中使用"eth0"作为网络接口名,实际使用时需要根据系统情况修改(可能是"enp0s3"、"ens33"等)。使用`ip a`命令查看可用的网络接口。

3. **防火墙配置**: 确保防火墙规则不会阻止实验流量:
   ```bash
   sudo ufw status
   sudo ufw allow 12345/udp
   sudo ufw allow 54321/udp
   ```

4. **MAC地址配置**: 示例程序中硬编码了MAC地址,实际使用时需要替换为真实的目标主机MAC地址。可以使用以下命令查看:
   ```bash
   ip link show
   arp -a
   ```

5. **路由转发功能**: 在Linux系统中启用IP转发:
   ```bash
   sudo sysctl -w net.ipv4.ip_forward=1
   ```

### 调试工具

1. **tcpdump**: 捕获和分析网络数据包
   ```bash
   sudo tcpdump -i eth0 -n
   ```

2. **Wireshark**: 图形化的网络数据包分析工具
   ```bash
   sudo wireshark
   ```

3. **ping**: 测试网络连通性
   ```bash
   ping 192.168.1.1
   ```

4. **traceroute**: 追踪数据包路由路径
   ```bash
   traceroute 192.168.2.2
   ```

### 实验注意事项

1. **按顺序启动**: 严格按照实验步骤中指定的顺序启动程序(通常是先启动接收程序,再启动转发程序,最后启动发送程序)

2. **检查网络配置**: 在运行程序前,确保所有主机的IP地址、子网掩码、路由表配置正确

3. **保存实验数据**: 使用tcpdump或Wireshark保存数据包捕获文件,用于实验报告分析

4. **安全性考虑**: 原始套接字具有较高的权限,不要在生产环境或公共网络中运行未经充分测试的代码

5. **代码备份**: 及时保存和备份实验代码,建议使用版本控制系统(如Git)

---

## 扩展实验建议

### 选做实验1: 多消息发送

修改UDP数据报收发程序,实现:
- 从控制台读取用户输入的消息
- 循环发送多条消息
- 在接收端显示所有接收到的消息及其序号

### 选做实验2: 扩展网络规模

将网络拓扑扩展到5台或更多主机:
- 设计更复杂的网络拓扑结构
- 实现通用的转发表机制
- 支持多跳路由转发
- 添加路由表动态更新功能

### 选做实验3: 双向传输

完善双网口路由器程序:
- 实现双向路由表查找
- 支持从目的主机到源主机的回程数据包转发
- 实现往返时间(RTT)测量
- 添加数据包丢失检测和重传机制

### 高级扩展

1. **ARP协议实现**: 
   - 实现ARP请求和响应
   - 维护ARP缓存表
   - 自动获取目标MAC地址

2. **ICMP协议支持**:
   - 实现ICMP回显请求/应答(ping功能)
   - 处理TTL超时(traceroute功能)
   - 生成ICMP差错报文

3. **性能优化**:
   - 使用多线程处理数据包
   - 实现数据包队列缓冲
   - 优化校验和计算

4. **监控和统计**:
   - 记录转发的数据包数量
   - 统计不同协议类型的流量
   - 生成流量分析报告

---

## 参考资源

### 在线文档

- Linux Socket Programming: https://man7.org/linux/man-pages/
- Beej's Guide to Network Programming: https://beej.us/guide/bgnet/
- TCP/IP详解 (Stevens)

### 相关RFC文档

- RFC 791: Internet Protocol (IP)
- RFC 792: Internet Control Message Protocol (ICMP)
- RFC 768: User Datagram Protocol (UDP)
- RFC 826: Ethernet Address Resolution Protocol (ARP)

### 实用工具文档

- tcpdump: https://www.tcpdump.org/manpages/tcpdump.1.html
- Wireshark: https://www.wireshark.org/docs/
- iptables: https://linux.die.net/man/8/iptables

---

## 附录: 常用网络命令速查

### 网络配置命令

```bash
# 查看网络接口信息
ip addr show
ifconfig

# 查看路由表
ip route show
route -n

# 添加静态路由
sudo ip route add 192.168.2.0/24 via 192.168.1.1
sudo route add -net 192.168.2.0/24 gw 192.168.1.1

# 删除路由
sudo ip route del 192.168.2.0/24
sudo route del -net 192.168.2.0/24

# 启用IP转发
sudo sysctl -w net.ipv4.ip_forward=1
# 永久启用(编辑/etc/sysctl.conf)
echo "net.ipv4.ip_forward=1" | sudo tee -a /etc/sysctl.conf

# 查看ARP缓存
arp -a
ip neigh show
```

### 网络测试命令

```bash
# 测试连通性
ping -c 4 192.168.1.1

# 追踪路由
traceroute 192.168.2.2
tracepath 192.168.2.2

# 查看网络连接
netstat -an
ss -tuln

# 监听端口
sudo netstat -tulpn | grep 12345
sudo ss -tulpn | grep 12345
```

### 数据包捕获命令

```bash
# 捕获所有数据包
sudo tcpdump -i eth0

# 捕获指定协议
sudo tcpdump -i eth0 icmp
sudo tcpdump -i eth0 udp port 12345

# 保存到文件
sudo tcpdump -i eth0 -w capture.pcap

# 读取捕获文件
tcpdump -r capture.pcap

# 显示详细信息
sudo tcpdump -i eth0 -v
sudo tcpdump -i eth0 -vv
sudo tcpdump -i eth0 -vvv

# 显示十六进制和ASCII
sudo tcpdump -i eth0 -X
```

### 防火墙命令

```bash
# 查看防火墙状态
sudo ufw status

# 允许端口
sudo ufw allow 12345/udp
sudo ufw allow 54321/udp

# 禁用防火墙(仅用于实验)
sudo ufw disable

# 启用防火墙
sudo ufw enable
```

---

**实验指导书结束**

*祝实验顺利!如有问题,请及时与指导教师沟通。* = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (len > 0) {
            printf("Received packet of length %d\n", len);
        }
    }

    close(sockfd);
    return 0;
}
```

#### AF_INET原始套接字

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    char buffer[2048];
    while (1) {
        int len
    return 0;
}
```

编译并运行:
```bash
gcc -o send_ip send_ip.c
./send_ip
```

**3. 转发IP数据报**

创建文件`forward_ip.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sockfd;
    struct sockaddr_in src_addr, dest_addr, my_addr;
    char buffer[1024];
    socklen_t addr_len;
    int src_port = 12345;   // 原始端口号
    int dest_port = 54321;  // 目标端口号

    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 本地地址
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(src_port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字到本地地址
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("bind");
        return 1;
    }

    // 接收数据报
    addr_len = sizeof(src_addr);
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                 (struct sockaddr *)&src_addr, &addr_len) < 0) {
        perror("recvfrom");
        return 1;
    }
    printf("Datagram received: %s\n", buffer);

    // 修改目标地址为接收程序主机的IP地址
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    dest_addr.sin_addr.s_addr = inet_addr("接收程序的IP地址");

    // 发送数据报
    if (sendto(sockfd, buffer, strlen(buffer), 0, 
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("sendto");
        return 1;
    }

    printf("Datagram forwarded.\n");
    close(sockfd);
    return 0;
}
```

开放防火墙端口并编译运行:
```bash
sudo ufw allow 12345/udp
gcc -o forward_ip forward_ip.c
./forward_ip
```

**4. 接收IP数据报**

创建文件`recv_ip.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sockfd;
    struct sockaddr_in src_addr, my_addr;
    char buffer[1024];
    socklen_t addr_len;
    int port = 54321;  // 接收端口号

    // 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 本地地址
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字到本地地址
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("bind");
        return 1;
    }

    // 接收数据报
    addr_len = sizeof(src_addr);
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                 (struct sockaddr *)&src_addr, &addr_len) < 0) {
        perror("recvfrom");
        return 1;
    }

    printf("Datagram received: %s\n", buffer);
    close(sockfd);
    return 0;
}
```

开放防火墙端口并编译运行:
```bash
sudo ufw allow 54321/udp
gcc -o recv_ip recv_ip.c
./recv_ip
```

---

### 4.2 基于单网口主机的IP数据报转发及收发

#### 4.2.1 实验环境

**网络拓扑说明**:

1. 源主机(192.168.1.1/24): 发送数据包到目的主机(192.168.1.3)
2. 转发主机(192.168.1.2/24): 接收并转发数据包
3. 目的主机(192.168.1.3/24): 接收数据包

**特殊处理技巧**:

由于源主机(192.168.1.1)发送的IP数据包目的地址为接收主机(192.168.1.3),二者处于同一子网中,正常情况下IP数据报将直接交付给目的主机,不会经由转发主机(192.168.1.2)转发。

为解决这个问题,在发送主机上:
1. 构造IP数据报,源IP为192.168.1.1,目的IP为192.168.1.3
2. 构造以太网帧时,源MAC地址为aa:aa:aa:aa:aa:aa,但目的MAC地址设为转发主机的MAC地址bb:bb:bb:bb:bb:bb(而非接收主机的MAC地址)
3. 通过这种方法,将去往接收主机的IP数据报交给转发主机进行转发

实际上路由器转发过程与此类似,通过源MAC地址和目的MAC地址的变化,实现IP数据报从源主机到目的主机及传输路径上各路由器间的跳转传输。

**数据包处理流程**:

1. 源主机发送数据包,以太网帧的目的MAC地址为路由主机的MAC地址
2. 路由器接收数据包,解析IP头部信息,修改TTL字段并重新计算校验和
3. 路由器根据路由表决定将数据包转发到目的主机
4. 目的主机接收数据包并处理

#### 4.2.2 三个主机的示例程序

**1. 发送主机程序(源主机)**

该程序构造以太网帧,IP头的源地址和目的地址分别为192.168.1.2和192.168.1.3,以太网头的目的MAC地址为路由主机的MAC地址。

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>

#define DEST_MAC0 0x00
#define DEST_MAC1 0x0c
#define DEST_MAC2 0x29
#define DEST_MAC3 0x3e
#define DEST_MAC4 0x1e
#define DEST_MAC5 0x4c
#define ETHER_TYPE 0x0800
#define BUFFER_SIZE 1518
#define UDP_SRC_PORT 12345
#define UDP_DST_PORT 12345

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main() {
    int sockfd;
    struct ifreq if_idx, if_mac;
    struct sockaddr_ll socket_address;
    char buffer[BUFFER_SIZE];

    // 创建原始套接字
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        perror("socket");
        return 1;
    }

    // 获取接口索引
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, "eth0", IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        perror("SIOCGIFINDEX");
        return 1;
    }

    // 获取接口MAC地址
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, "eth0", IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
        perror("SIOCGIFHWADDR");
        return 1;
    }

    // 构造以太网头
    struct ether_header *eh = (struct ether_header *) buffer;
    eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
    eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
    eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
    eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
    eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
    eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
    eh->ether_dhost[0] = DEST_MAC0;
    eh->ether_dhost[1] = DEST_MAC1;
    eh->ether_dhost[2] = DEST_MAC2;
    eh->ether_dhost[3] = DEST_MAC3;
    eh->ether_dhost[4] = DEST_MAC4;
    eh->ether_dhost[5] = DEST_MAC5;
    eh->ether_type = htons(ETHER_TYPE);

    // 构造IP头
    struct iphdr *iph = (struct iphdr *) (buffer + sizeof(struct ether_header));
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + 
                         strlen("Hello, this is a test message."));
    iph->id = htonl(54321);
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_UDP;
    iph->check = 0;
    iph->saddr = inet_addr("192.168.1.2");
    iph->daddr = inet_addr("192.168.1.3");
    iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

    // 构造UDP头
    struct udphdr *udph = (struct udphdr *) (buffer + sizeof(struct ether_header) + 
                                             sizeof(struct iphdr));
    udph->source = htons(UDP_SRC_PORT);
    udph->dest = htons(UDP_DST_PORT);
    udph->len = htons(sizeof(struct udphdr) + strlen("Hello, this is a test message."));
    udph->check = 0;  // UDP校验和可选

    // 填充数据
    char *data = (char *) (buffer + sizeof(struct ether_header) + 
                          sizeof(struct iphdr) + sizeof(struct udphdr));
    strcpy(data, "Hello, this is a test message.");

    // 设置socket地址结构
    socket_address.sll_ifindex = if_idx.ifr_ifindex;
    socket_address.sll_halen = ETH_ALEN;
    socket_address.sll_addr[0] = DEST_MAC0;
    socket_address.sll_addr[1] = DEST_MAC1;
    socket_address.sll_addr[2] = DEST_MAC2;
    socket_address.sll_addr[3] = DEST_MAC3;
    socket_address.sll_addr[4] = DEST_MAC4;
    socket_address.sll_addr[5] = DEST_MAC5;

    // 发送数据包
    if (sendto(sockfd, buffer, sizeof(struct ether_header) + sizeof(struct iphdr) + 
               sizeof(struct udphdr) + strlen("Hello, this is a test message."), 0, 
               (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
        perror("sendto");
        return 1;
    }

    close(sockfd);
    return 0;
}
```

**程序说明**:

1. 使用`socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))`创建原始套接字
2. 使用`ioctl`获取网络接口的索引和MAC地址
3. 构造以太网头,设置源MAC地址和目的MAC地址(路由主机的MAC地址)
4. 构造IP头,设置源IP地址和目的IP地址
5. 构造UDP头,设置源端口和目的端口
6. 配置socket地址结构并使用`sendto`发送数据包

**2. 路由转发程序(中间主机)**

该程序捕获数据包,修改TTL并转发到目的主机:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <time.h>

#define BUFFER_SIZE 65536

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main() {
    int sockfd;
    struct sockaddr saddr;
    unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    while (1) {
        int saddr_len = sizeof(saddr);
        int data_size = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, &saddr, 
                                (socklen_t *)&saddr_len);
        if (data_size < 0) {
            perror("Recvfrom error");
            return 1;
        }

        struct ethhdr *eth_header = (struct ethhdr *)buffer;
        struct iphdr *ip_header = (struct iphdr *)(buffer + sizeof(struct ethhdr));

        char src_ip[INET_ADDRSTRLEN];
        char dest_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_header->saddr), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(ip_header->daddr), dest_ip, INET_ADDRSTRLEN);

        if (strcmp(src_ip, "192.168.1.1") == 0 && strcmp(dest_ip, "192.168.1.3") == 0) {
            // 获取当前系统时间
            time_t rawtime;
            struct tm *timeinfo;
            char time_str[100];
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

            printf("[%s] Captured packet from %s to %s\n", time_str, src_ip, dest_ip);

            // 修改TTL
            ip_header->ttl -= 1;
            ip_header->check = 0;
            ip_header->check = checksum((unsigned short *)ip_header, ip_header->ihl * 4);

            // 发送数据包到目的主机
            struct ifreq ifr, ifr_mac;
            struct sockaddr_ll dest;

            // 获取网卡接口索引
            memset(&ifr, 0, sizeof(ifr));
            snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "eth0");
            if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
                perror("ioctl");
                return 1;
            }

            // 获取网卡接口MAC地址
            memset(&ifr_mac, 0, sizeof(ifr_mac));
            snprintf(ifr_mac.ifr_name, sizeof(ifr_mac.ifr_name), "eth0");
            if (ioctl(sockfd, SIOCGIFHWADDR, &ifr_mac) < 0) {
                perror("ioctl");
                return 1;
            }

            // 设置目标MAC地址
            unsigned char target_mac[ETH_ALEN] = {0x00, 0x0c, 0x29, 0x48, 0xd3, 0xf7};
            memset(&dest, 0, sizeof(dest));
            dest.sll_ifindex = ifr.ifr_ifindex;
            dest.sll_halen = ETH_ALEN;
            memcpy(dest.sll_addr, target_mac, ETH_ALEN);

            // 构造新的以太网帧头
            memcpy(eth_header->h_dest, target_mac, ETH_ALEN);
            memcpy(eth_header->h_source, ifr_mac.ifr_hwaddr.sa_data, ETH_ALEN);
            eth_header->h_proto = htons(ETH_P_IP);

            printf("Interface name: %s, index: %d\n", ifr.ifr_name, ifr.ifr_ifindex);

            if (sendto(sockfd, buffer, data_size, 0, (struct sockaddr *)&dest, 
                      sizeof(dest)) < 0) {
                perror("Sendto error");
                return 1;
            }
            printf("Datagram forwarded.\n");
        } else {
            printf("Ignored packet from %s to %s\n", src_ip, dest_ip);
        }
    }

    close(sockfd);