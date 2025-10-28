/*
 * 实验4.2 - 转发端程序（主机Shorekeeper）
 * 功能：使用AF_PACKET原始套接字接收完整以太网帧并转发
 * 编译：gcc -o forward_raw forward_raw.c
 * 运行：sudo ./forward_raw
 *
 * 注意：
 * 1. 需要ROOT权限运行
 * 2. 需要修改接收主机Jinhsi的MAC地址
 * 3. 使用 ip a 命令查看网卡接口名称（如eth0、ens33等）
 */

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
#include <signal.h>

#define BUFFER_SIZE 1518
#define UDP_PORT 12345
#define INTERFACE "ens33"           // 网卡接口名，需要根据实际情况修改

// 网络配置
#define SRC_IP "192.168.10.100"     // Camellya的IP（过滤用）
#define DST_IP "192.168.10.102"     // Jinhsi的IP（最终目的地）

// ⚠️ 重要：需要修改为接收主机Jinhsi的实际MAC地址
// 使用命令查看：在Jinhsi上执行 ip a
// 00:0c:29:69:e1:6b
#define DEST_MAC0 0x00
#define DEST_MAC1 0x0c
#define DEST_MAC2 0x29
#define DEST_MAC3 0x69
#define DEST_MAC4 0xe1
#define DEST_MAC5 0x6b

// 全局变量用于信号处理
static volatile int keep_running = 1;
static int msg_count = 0;

// 信号处理函数
void signal_handler(int signum) {
    printf("\n\n接收到退出信号 (Ctrl+C)\n");
    keep_running = 0;
}

// 计算校验和
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

    printf("===========================================\n");
    printf("  原始套接字转发程序 - Shorekeeper\n");
    printf("===========================================\n");
    printf("源IP过滤: %s (Camellya)\n", SRC_IP);
    printf("目的IP: %s (Jinhsi)\n", DST_IP);
    printf("网卡接口: %s\n", INTERFACE);
    printf("⚠️  需要ROOT权限运行！\n");
    printf("===========================================\n\n");

    // 注册信号处理
    signal(SIGINT, signal_handler);

    // 创建原始套接字
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        perror("socket");
        printf("\n错误：需要ROOT权限！请使用 sudo 运行\n");
        return 1;
    }

    // 获取接口索引
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, INTERFACE, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        perror("SIOCGIFINDEX");
        printf("\n错误：找不到网卡接口 %s\n", INTERFACE);
        printf("提示：使用 'ip a' 命令查看可用的网卡接口名称\n");
        close(sockfd);
        return 1;
    }

    // 获取接口MAC地址
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, INTERFACE, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
        perror("SIOCGIFHWADDR");
        close(sockfd);
        return 1;
    }

    printf("本机MAC地址: %02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned char)if_mac.ifr_hwaddr.sa_data[0],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[1],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[2],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[3],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[4],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[5]);

    printf("目标MAC地址: %02x:%02x:%02x:%02x:%02x:%02x (Jinhsi)\n",
           DEST_MAC0, DEST_MAC1, DEST_MAC2, DEST_MAC3, DEST_MAC4, DEST_MAC5);
    printf("\n等待接收数据包... (按 Ctrl+C 退出)\n");
    printf("===========================================\n\n");

    // 持续接收和转发
    while (keep_running) {
        memset(buffer, 0, BUFFER_SIZE);

        ssize_t recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);

        if (recv_len < 0) {
            continue;
        }

        // 解析以太网帧头
        struct ether_header *eh = (struct ether_header *)buffer;

        // 只处理IP数据包
        if (ntohs(eh->ether_type) != ETH_P_IP) {
            continue;
        }

        // 解析IP头
        struct iphdr *iph = (struct iphdr *)(buffer + sizeof(struct ether_header));

        // 过滤：只处理来自Camellya且目标是Jinhsi的UDP数据包
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(iph->saddr), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(iph->daddr), dst_ip, INET_ADDRSTRLEN);

        if (iph->protocol != IPPROTO_UDP) {
            continue;
        }

        if (strcmp(src_ip, SRC_IP) != 0 || strcmp(dst_ip, DST_IP) != 0) {
            continue;
        }

        // 解析UDP头
        struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct ether_header) +
                                                 sizeof(struct iphdr));

        // 检查目标端口
        if (ntohs(udph->dest) != UDP_PORT) {
            continue;
        }

        // 提取数据
        char *data = (char *)(buffer + sizeof(struct ether_header) +
                             sizeof(struct iphdr) + sizeof(struct udphdr));
        int data_len = ntohs(iph->tot_len) - sizeof(struct iphdr) - sizeof(struct udphdr);
        data[data_len] = '\0';

        msg_count++;
        printf("[消息 %d] 收到来自 %s\n", msg_count, src_ip);
        printf("  内容: \"%s\"\n", data);
        printf("  长度: %d 字节\n", data_len);
        printf("  原TTL: %d\n", iph->ttl);

        // 修改以太网帧头：更新MAC地址
        // 源MAC：本机MAC
        memcpy(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, 6);
        // 目标MAC：接收主机Jinhsi的MAC
        eh->ether_dhost[0] = DEST_MAC0;
        eh->ether_dhost[1] = DEST_MAC1;
        eh->ether_dhost[2] = DEST_MAC2;
        eh->ether_dhost[3] = DEST_MAC3;
        eh->ether_dhost[4] = DEST_MAC4;
        eh->ether_dhost[5] = DEST_MAC5;

        // 修改IP头：递减TTL并重新计算校验和
        iph->ttl--;
        iph->check = 0;
        iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

        printf("  新TTL: %d\n", iph->ttl);

        // 设置socket地址结构
        memset(&socket_address, 0, sizeof(socket_address));
        socket_address.sll_ifindex = if_idx.ifr_ifindex;
        socket_address.sll_halen = ETH_ALEN;
        socket_address.sll_addr[0] = DEST_MAC0;
        socket_address.sll_addr[1] = DEST_MAC1;
        socket_address.sll_addr[2] = DEST_MAC2;
        socket_address.sll_addr[3] = DEST_MAC3;
        socket_address.sll_addr[4] = DEST_MAC4;
        socket_address.sll_addr[5] = DEST_MAC5;

        // 转发数据包
        if (sendto(sockfd, buffer, recv_len, 0,
                   (struct sockaddr *)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
            perror("  ✗ 转发失败");
            continue;
        }

        printf("  ✓ 已转发到 %s (%02x:%02x:%02x:%02x:%02x:%02x)\n\n",
               DST_IP, DEST_MAC0, DEST_MAC1, DEST_MAC2, DEST_MAC3, DEST_MAC4, DEST_MAC5);
    }

    printf("\n===========================================\n");
    printf("总共转发了 %d 个数据包\n", msg_count);
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
