/*
 * 实验4.2增强版 - 发送端程序（支持多目标选择）
 * 功能：使用AF_PACKET原始套接字构造完整的以太网帧发送UDP数据报
 * 编译：gcc -o send_raw_enhanced send_raw_enhanced.c
 * 运行：sudo ./send_raw_enhanced
 *
 * 改进点：
 * 1. 支持选择多个目标接收端
 * 2. 适配5台主机的网络拓扑
 * 3. 增强的用户交互界面
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

#define BUFFER_SIZE 1518
#define UDP_SRC_PORT 12345
#define UDP_DST_PORT 12345

// 网卡接口配置
#define INTERFACE "ens33"           // 网卡接口名，需要根据实际情况修改

// 转发器（第一跳：Phrolova）配置
#define FORWARD_HUB_IP "192.168.10.103"   // Phrolova的IP
#define FORWARD_HUB_MAC0 0x00
#define FORWARD_HUB_MAC1 0x0c
#define FORWARD_HUB_MAC2 0x29
#define FORWARD_HUB_MAC3 0x86
#define FORWARD_HUB_MAC4 0x81
#define FORWARD_HUB_MAC5 0xca

// 目标主机配置（最终接收端）
typedef struct {
    char name[32];
    char ip[16];
} TargetHost;

TargetHost targets[] = {
    {"Jinhsi", "192.168.10.102"}
};

const int num_targets = sizeof(targets) / sizeof(TargetHost);

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

// 显示目标选择菜单
int select_target() {
    int choice;

    printf("\n请选择接收端：\n");
    for (int i = 0; i < num_targets; i++) {
        printf("  [%d] %s (%s)\n", i + 1, targets[i].name, targets[i].ip);
    }
    printf("  [0] 返回主菜单\n");
    printf("请输入选择: ");
    fflush(stdout);

    scanf("%d", &choice);
    getchar();  // 消耗换行符

    if (choice >= 1 && choice <= num_targets) {
        return choice - 1;
    }
    return -1;
}

int main() {
    int sockfd;
    struct ifreq if_idx, if_mac;
    struct sockaddr_ll socket_address;
    char buffer[BUFFER_SIZE];
    char message[1024];
    int msg_count = 0;
    char src_ip[16];

    printf("===========================================\n");
    printf("  实验4.2增强版 - 发送端程序 (Camellya)\n");
    printf("===========================================\n");
    printf("第一跳转发器: %s (Phrolova)\n", FORWARD_HUB_IP);
    printf("最终目标: Jinhsi (192.168.10.102)\n");
    printf("网卡接口: %s\n", INTERFACE);
    printf("⚠️  需要ROOT权限运行！\n");
    printf("===========================================\n\n");

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

    // 获取本机IP地址
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0) {
        perror("SIOCGIFADDR");
        close(sockfd);
        return 1;
    }
    struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    strcpy(src_ip, inet_ntoa(ipaddr->sin_addr));

    printf("本机IP: %s\n", src_ip);
    printf("本机MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned char)if_mac.ifr_hwaddr.sa_data[0],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[1],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[2],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[3],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[4],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[5]);
    printf("转发器MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           FORWARD_HUB_MAC0, FORWARD_HUB_MAC1, FORWARD_HUB_MAC2,
           FORWARD_HUB_MAC3, FORWARD_HUB_MAC4, FORWARD_HUB_MAC5);

    // 主循环
    while (1) {
        // 选择目标
        int target_idx = select_target();
        if (target_idx < 0) {
            printf("\n程序退出\n");
            break;
        }

        printf("\n已选择接收端: %s (%s)\n",
               targets[target_idx].name, targets[target_idx].ip);
        printf("输入消息进行发送，输入 'back' 返回菜单\n");
        printf("-------------------------------------------\n\n");

        // 向选定目标发送消息
        while (1) {
            printf("消息 [%d]: ", msg_count + 1);
            fflush(stdout);

            // 读取用户输入
            if (fgets(message, sizeof(message), stdin) == NULL) {
                printf("\n读取输入失败\n");
                break;
            }

            // 移除换行符
            size_t len = strlen(message);
            if (len > 0 && message[len - 1] == '\n') {
                message[len - 1] = '\0';
                len--;
            }

            // 检查返回命令
            if (strcmp(message, "back") == 0) {
                break;
            }

            // 检查空消息
            if (len == 0) {
                printf("消息不能为空\n");
                continue;
            }

            // 清空缓冲区
            memset(buffer, 0, BUFFER_SIZE);

            // 构造以太网帧头 - 目标MAC是转发器
            struct ether_header *eh = (struct ether_header *)buffer;
            memcpy(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, 6);
            eh->ether_dhost[0] = FORWARD_HUB_MAC0;
            eh->ether_dhost[1] = FORWARD_HUB_MAC1;
            eh->ether_dhost[2] = FORWARD_HUB_MAC2;
            eh->ether_dhost[3] = FORWARD_HUB_MAC3;
            eh->ether_dhost[4] = FORWARD_HUB_MAC4;
            eh->ether_dhost[5] = FORWARD_HUB_MAC5;
            eh->ether_type = htons(ETH_P_IP);

            // 构造IP头 - 目标IP是最终接收端
            struct iphdr *iph = (struct iphdr *)(buffer + sizeof(struct ether_header));
            iph->ihl = 5;
            iph->version = 4;
            iph->tos = 0;
            iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + strlen(message));
            iph->id = htonl(54321);
            iph->frag_off = 0;
            iph->ttl = 255;
            iph->protocol = IPPROTO_UDP;
            iph->check = 0;
            iph->saddr = inet_addr(src_ip);
            iph->daddr = inet_addr(targets[target_idx].ip);  // 最终目标IP
            iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

            // 构造UDP头
            struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct ether_header) +
                                                     sizeof(struct iphdr));
            udph->source = htons(UDP_SRC_PORT);
            udph->dest = htons(UDP_DST_PORT);
            udph->len = htons(sizeof(struct udphdr) + strlen(message));
            udph->check = 0;

            // 填充数据
            char *data = (char *)(buffer + sizeof(struct ether_header) +
                                 sizeof(struct iphdr) + sizeof(struct udphdr));
            strcpy(data, message);

            // 设置socket地址结构
            memset(&socket_address, 0, sizeof(socket_address));
            socket_address.sll_ifindex = if_idx.ifr_ifindex;
            socket_address.sll_halen = ETH_ALEN;
            socket_address.sll_addr[0] = FORWARD_HUB_MAC0;
            socket_address.sll_addr[1] = FORWARD_HUB_MAC1;
            socket_address.sll_addr[2] = FORWARD_HUB_MAC2;
            socket_address.sll_addr[3] = FORWARD_HUB_MAC3;
            socket_address.sll_addr[4] = FORWARD_HUB_MAC4;
            socket_address.sll_addr[5] = FORWARD_HUB_MAC5;

            // 发送数据包
            int total_len = sizeof(struct ether_header) + sizeof(struct iphdr) +
                           sizeof(struct udphdr) + strlen(message);

            if (sendto(sockfd, buffer, total_len, 0,
                       (struct sockaddr *)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
                perror("sendto");
                continue;
            }

            msg_count++;
            printf("✓ 已发送到 %s: \"%s\" (%ld 字节)\n\n",
                   targets[target_idx].name, message, strlen(message));

            usleep(100000);  // 0.1秒延迟
        }
    }

    printf("\n===========================================\n");
    printf("总共发送了 %d 个数据包\n", msg_count);
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
