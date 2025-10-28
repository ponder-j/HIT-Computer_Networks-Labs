/*
 * 实验4.2增强版 - 转发端程序（基于转发表的通用转发器）
 * 功能：使用AF_PACKET原始套接字和转发表实现IP数据报转发
 * 编译：gcc -o forward_raw_enhanced forward_raw_enhanced.c
 * 运行：sudo ./forward_raw_enhanced
 *
 * 关键改进：
 * 1. 使用转发表（Forwarding Table）进行路由决策
 * 2. 支持多个发送端和多个接收端
 * 3. 增加程序通用性，便于扩展到更多主机
 * 4. 显示详细的转发统计信息
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
#include <time.h>

#define BUFFER_SIZE 1518
#define UDP_PORT 12345
#define INTERFACE "ens33"
#define MAX_FORWARD_ENTRIES 10

// 转发表条目结构
typedef struct {
    char dest_ip[16];           // 目标IP地址
    unsigned char dest_mac[6];  // 目标MAC地址
    char description[32];        // 描述信息
    int packet_count;            // 转发数据包计数
} ForwardEntry;

// 转发表
ForwardEntry forward_table[MAX_FORWARD_ENTRIES];
int forward_table_size = 0;

// 统计信息
typedef struct {
    int total_received;
    int total_forwarded;
    int unknown_dest;
    int ttl_expired;
} ForwardStats;

ForwardStats stats = {0, 0, 0, 0};

// 全局变量用于信号处理
static volatile int keep_running = 1;

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

// 初始化转发表
void init_forward_table() {
    // ⚠️ 重要：这是通用转发器代码，需要根据部署的主机修改转发表
    //
    // 本配置用于: Carlotta (192.168.10.104) - 第三跳转发器（最后一跳）
    // 数据流: Camellya → Phrolova → Shorekeeper → Carlotta → Jinhsi

    // 条目1: 目标Jinhsi，下一跳就是Jinhsi本身（直接送达）
    strcpy(forward_table[0].dest_ip, "192.168.10.102");  // 最终目标IP (Jinhsi)
    forward_table[0].dest_mac[0] = 0x00;  // Jinhsi的MAC
    forward_table[0].dest_mac[1] = 0x0c;
    forward_table[0].dest_mac[2] = 0x29;
    forward_table[0].dest_mac[3] = 0x69;
    forward_table[0].dest_mac[4] = 0xe1;
    forward_table[0].dest_mac[5] = 0x6b;
    strcpy(forward_table[0].description, "Jinhsi");
    forward_table[0].packet_count = 0;

    forward_table_size = 1;

    // 可以继续添加更多条目...
}

// 在转发表中查找目标
ForwardEntry* lookup_forward_table(const char *dest_ip) {
    for (int i = 0; i < forward_table_size; i++) {
        if (strcmp(forward_table[i].dest_ip, dest_ip) == 0) {
            return &forward_table[i];
        }
    }
    return NULL;
}

// 显示转发表
void print_forward_table() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                      转发表 (Forwarding Table)                 ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ 序号 │ 目标IP          │ 目标MAC           │     描述          ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");

    for (int i = 0; i < forward_table_size; i++) {
        printf("║  %2d  │ %-15s │ %02x:%02x:%02x:%02x:%02x:%02x │ %-9s         ║\n",
               i + 1,
               forward_table[i].dest_ip,
               forward_table[i].dest_mac[0],
               forward_table[i].dest_mac[1],
               forward_table[i].dest_mac[2],
               forward_table[i].dest_mac[3],
               forward_table[i].dest_mac[4],
               forward_table[i].dest_mac[5],
               forward_table[i].description);
    }

    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
}

// 显示统计信息
void print_statistics() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
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

// 获取时间字符串
void get_time_string(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, size, "%H:%M:%S", t);
}

int main() {
    int sockfd;
    struct ifreq if_idx, if_mac;
    struct sockaddr_ll socket_address;
    char buffer[BUFFER_SIZE];
    char time_str[20];

    printf("===========================================\n");
    printf("  实验4.2增强版 - 通用转发器\n");
    printf("  基于转发表的IP数据报转发\n");
    printf("===========================================\n");
    printf("⚠️  请确认当前主机身份并修改转发表！\n");
    printf("网卡接口: %s\n", INTERFACE);
    printf("⚠️  需要ROOT权限运行！\n");
    printf("===========================================\n");

    // 初始化转发表
    init_forward_table();

    // 显示转发表
    print_forward_table();

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

        // 解决循环捕获问题：如果数据包的源MAC是本机MAC，则忽略
        if (memcmp(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, 6) == 0) {
            continue;
        }

        // 只处理IP数据包
        if (ntohs(eh->ether_type) != ETH_P_IP) {
            continue;
        }

        // 解析IP头
        struct iphdr *iph = (struct iphdr *)(buffer + sizeof(struct ether_header));

        // 过滤：只处理UDP数据包
        if (iph->protocol != IPPROTO_UDP) {
            continue;
        }

        // 获取源IP和目标IP
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(iph->saddr), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(iph->daddr), dst_ip, INET_ADDRSTRLEN);

        // 过滤掉发给自己的包（避免循环）
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

        // 解析UDP头
        struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct ether_header) +
                                                 sizeof(struct iphdr));

        // 检查目标端口
        if (ntohs(udph->dest) != UDP_PORT) {
            continue;
        }

        stats.total_received++;

        // 提取数据
        char *data = (char *)(buffer + sizeof(struct ether_header) +
                             sizeof(struct iphdr) + sizeof(struct udphdr));
        int data_len = ntohs(iph->tot_len) - sizeof(struct iphdr) - sizeof(struct udphdr);
        if (data_len > 0 && data_len < BUFFER_SIZE) {
            data[data_len] = '\0';
        }

        get_time_string(time_str, sizeof(time_str));

        printf("[%s] 收到数据包\n", time_str);
        printf("  来源: %s\n", src_ip);
        printf("  目标: %s\n", dst_ip);
        printf("  内容: \"%s\"\n", data_len > 0 ? data : "(无数据)");
        printf("  原TTL: %d\n", iph->ttl);

        // 检查TTL
        if (iph->ttl <= 1) {
            printf("  ✗ TTL过期，丢弃数据包\n\n");
            stats.ttl_expired++;
            continue;
        }

        // 在转发表中查找目标
        ForwardEntry *entry = lookup_forward_table(dst_ip);

        if (entry == NULL) {
            printf("  ✗ 未知目标，无法转发\n\n");
            stats.unknown_dest++;
            continue;
        }

        printf("  → 查找转发表: 找到目标 %s\n", entry->description);

        // 修改以太网帧头：更新目标MAC地址和源MAC地址
        memcpy(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, 6);
        memcpy(eh->ether_dhost, entry->dest_mac, 6);  // 必须更新目标MAC！

        // 修改IP头：递减TTL并重新计算校验和
        iph->ttl--;
        iph->check = 0;
        iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

        printf("  新TTL: %d\n", iph->ttl);

        // 设置socket地址结构
        memset(&socket_address, 0, sizeof(socket_address));
        socket_address.sll_ifindex = if_idx.ifr_ifindex;
        socket_address.sll_halen = ETH_ALEN;
        memcpy(socket_address.sll_addr, entry->dest_mac, 6);

        // 转发数据包
        if (sendto(sockfd, buffer, recv_len, 0,
                   (struct sockaddr *)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
            perror("  ✗ 转发失败");
            continue;
        }

        stats.total_forwarded++;
        entry->packet_count++;

        printf("  ✓ 已转发到 %s (%s)\n",
               entry->description, dst_ip);
        printf("  目标MAC: %02x:%02x:%02x:%02x:%02x:%02x\n\n",
               entry->dest_mac[0], entry->dest_mac[1], entry->dest_mac[2],
               entry->dest_mac[3], entry->dest_mac[4], entry->dest_mac[5]);
    }

    // 显示统计信息
    print_statistics();

    printf("\n程序正常退出\n");
    close(sockfd);
    return 0;
}
