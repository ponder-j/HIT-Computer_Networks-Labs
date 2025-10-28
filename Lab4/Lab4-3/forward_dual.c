/*
 * 实验4.3 - 路由转发程序（双网口主机）
 * 功能：使用AF_PACKET原始套接字实现真正的路由转发
 * 编译：gcc -o forward_dual forward_dual.c
 * 运行：sudo ./forward_dual
 *
 * 网络配置：
 * ens37: 192.168.1.1/24（连接源主机网段）
 * ens38: 192.168.2.1/24（连接目的主机网段）
 *
 * 注意：
 * 1. 需要ROOT权限运行
 * 2. 需要修改目标MAC地址为实际的目的主机MAC地址
 * 3. 需要启用IP转发：sudo sysctl -w net.ipv4.ip_forward=1
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
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 65536

// 路由表条目结构
struct route_entry {
    uint32_t dest;          // 目的网络
    uint32_t gateway;       // 下一跳（0表示直接交付）
    uint32_t netmask;       // 子网掩码
    char interface[IFNAMSIZ]; // 出接口
};

// 路由表（地址使用运行时初始化，因为 inet_addr() 不是编译时常量）
struct route_entry route_table[] = {
    // 占位项，稍后在 main() 中使用 inet_addr() 初始化
    {0, 0, 0, ""}
};

int route_table_size = sizeof(route_table) / sizeof(route_table[0]);

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

// 路由表查找
struct route_entry *lookup_route(uint32_t dest_ip) {
    for (int i = 0; i < route_table_size; i++) {
        if ((dest_ip & route_table[i].netmask) ==
            (route_table[i].dest & route_table[i].netmask)) {
            return &route_table[i];
        }
    }
    return NULL;
}

// 获取当前时间字符串
void get_time_string(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, size, "%H:%M:%S", t);
}

int main() {
    int sockfd;
    struct sockaddr saddr;
    unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);
    char time_str[20];

    printf("===========================================\n");
    printf("  双网口路由转发程序 - 路由主机\n");
    printf("  (实验4.3 - 基于静态路由表)\n");
    printf("===========================================\n");
    printf("接口配置:\n");
    printf("  ens37: 192.168.1.1/24 (连接源主机)\n");
    printf("  ens38: 192.168.2.1/24 (连接目的主机)\n");
    printf("路由表:\n");
    // 初始化路由表地址（inet_addr 在运行时调用）
    // 如果需要更多路由，可扩展为读取配置或在此处增加赋值
    route_table[0].dest = inet_addr("192.168.2.0");
    route_table[0].netmask = inet_addr("255.255.255.0");
    strncpy(route_table[0].interface, "ens38", IFNAMSIZ - 1);
    route_table[0].interface[IFNAMSIZ - 1] = '\0';
    for (int i = 0; i < route_table_size; i++) {
        struct in_addr dest, mask;
        dest.s_addr = route_table[i].dest;
        mask.s_addr = route_table[i].netmask;
        printf("  %s/%s -> %s\n",
               inet_ntoa(dest), inet_ntoa(mask), route_table[i].interface);
    }
    printf("⚠️  需要ROOT权限运行！\n");
    printf("===========================================\n\n");

    // 注册信号处理
    signal(SIGINT, signal_handler);

    // 创建原始套接字
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd < 0) {
        perror("socket");
        printf("\n错误：需要ROOT权限！请使用 sudo 运行\n");
        free(buffer);
        return 1;
    }

    printf("开始监听数据包... (按 Ctrl+C 退出)\n");
    printf("===========================================\n\n");

    // 持续接收和转发
    while (keep_running) {
        int saddr_len = sizeof(saddr);
        int data_size = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, &saddr,
                                (socklen_t *)&saddr_len);

        if (data_size < 0) {
            continue;
        }

        // 解析以太网帧头
        struct ethhdr *eth_header = (struct ethhdr *)buffer;

        // 只处理IP数据包
        if (ntohs(eth_header->h_proto) != ETH_P_IP) {
            continue;
        }

        // 解析IP头
        struct iphdr *ip_header = (struct iphdr *)(buffer + sizeof(struct ethhdr));

        // 转换IP地址为字符串
        char src_ip[INET_ADDRSTRLEN];
        char dest_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_header->saddr), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(ip_header->daddr), dest_ip, INET_ADDRSTRLEN);

        // 只处理UDP协议
        if (ip_header->protocol != IPPROTO_UDP) {
            continue;
        }

        // 过滤：只处理来自192.168.1.0/24且目标是192.168.2.0/24的数据包
        if ((ip_header->saddr & inet_addr("255.255.255.0")) != inet_addr("192.168.1.0") ||
            (ip_header->daddr & inet_addr("255.255.255.0")) != inet_addr("192.168.2.0")) {
            continue;
        }

        // 查找路由表
        struct route_entry *route = lookup_route(ip_header->daddr);
        if (route == NULL) {
            fprintf(stderr, "No route to host %s\n", dest_ip);
            continue;
        }

        msg_count++;
        get_time_string(time_str, sizeof(time_str));

        printf("[%s] [消息 %d] 路由转发\n", time_str, msg_count);
        printf("  源地址: %s\n", src_ip);
        printf("  目的地址: %s\n", dest_ip);
        printf("  原TTL: %d\n", ip_header->ttl);

        // 检查TTL
        if (ip_header->ttl <= 1) {
            printf("  ✗ TTL过期，丢弃数据包\n\n");
            continue;
        }

        // 修改TTL
        ip_header->ttl -= 1;
        ip_header->check = 0;
        ip_header->check = checksum((unsigned short *)ip_header, ip_header->ihl * 4);

        printf("  新TTL: %d\n", ip_header->ttl);
        printf("  出接口: %s\n", route->interface);

        // 获取出接口信息
        struct ifreq ifr, ifr_mac;
        struct sockaddr_ll dest;

        // 获取网卡接口索引
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, route->interface, IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            perror("  ioctl SIOCGIFINDEX");
            continue;
        }

        // 获取网卡接口MAC地址
        memset(&ifr_mac, 0, sizeof(ifr_mac));
        strncpy(ifr_mac.ifr_name, route->interface, IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFHWADDR, &ifr_mac) < 0) {
            perror("  ioctl SIOCGIFHWADDR");
            continue;
        }

        // ⚠️ 重要：需要修改为实际的目的主机MAC地址
        // 实际应用中应使用ARP协议获取目的主机或下一跳的MAC地址
        // 这里简化处理，需要手动配置
        unsigned char target_mac[ETH_ALEN] = {0x00, 0x0c, 0x29, 0x48, 0xd3, 0xf7};

        // 设置socket地址结构
        memset(&dest, 0, sizeof(dest));
        dest.sll_ifindex = ifr.ifr_ifindex;
        dest.sll_halen = ETH_ALEN;
        memcpy(dest.sll_addr, target_mac, ETH_ALEN);

        // 修改以太网帧头
        memcpy(eth_header->h_dest, target_mac, ETH_ALEN);  // 目标MAC
        memcpy(eth_header->h_source, ifr_mac.ifr_hwaddr.sa_data, ETH_ALEN);  // 源MAC
        eth_header->h_proto = htons(ETH_P_IP);  // 以太网类型

        // 发送数据包
        if (sendto(sockfd, buffer, data_size, 0,
                   (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            perror("  ✗ 转发失败");
            continue;
        }

        printf("  ✓ 已转发到 %s (通过 %s)\n", dest_ip, route->interface);
        printf("  目标MAC: %02x:%02x:%02x:%02x:%02x:%02x\n\n",
               target_mac[0], target_mac[1], target_mac[2],
               target_mac[3], target_mac[4], target_mac[5]);
    }

    printf("\n===========================================\n");
    printf("总共转发了 %d 个数据包\n", msg_count);
    printf("===========================================\n");

    close(sockfd);
    free(buffer);
    return 0;
}
