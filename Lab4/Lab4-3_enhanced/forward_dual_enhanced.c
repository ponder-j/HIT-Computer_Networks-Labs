/*
 * 实验4.3增强版 - 路由转发程序（双网口主机）
 * 功能：使用AF_PACKET原始套接字实现双向路由转发
 * 编译：gcc -o forward_dual_enhanced forward_dual_enhanced.c
 * 运行：sudo ./forward_dual_enhanced
 *
 * 网络配置：
 * eth0: 192.168.1.1/24（连接源主机网段）
 * eth1: 192.168.2.1/24（连接目的主机网段）
 *
 * 增强特性：
 * 1. 完善的双向路由表
 * 2. ARP缓存表支持
 * 3. 双向转发支持（192.168.1.x ↔ 192.168.2.x）
 * 4. 动态MAC地址解析
 * 5. 详细的日志输出
 *
 * 注意：
 * 1. 需要ROOT权限运行
 * 2. 需要启用IP转发：sudo sysctl -w net.ipv4.ip_forward=1
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
#define ARP_CACHE_SIZE 10

// 路由表条目结构
struct route_entry {
    uint32_t dest;          // 目的网络
    uint32_t gateway;       // 下一跳（0表示直接交付）
    uint32_t netmask;       // 子网掩码
    char interface[IFNAMSIZ]; // 出接口
    char description[64];   // 路由描述
};

// ARP缓存条目结构
struct arp_entry {
    uint32_t ip_addr;           // IP地址
    unsigned char mac[ETH_ALEN]; // MAC地址
    int valid;                   // 是否有效
};

// 增强的双向路由表
// 注意：inet_addr() 不能在静态初始化器中调用（不是常量），
// 因此在这里只声明数组大小，实际在运行时由 init_route_table() 填充。
struct route_entry route_table[2];

int route_table_size = sizeof(route_table) / sizeof(route_table[0]);

// ARP缓存表
struct arp_entry arp_cache[ARP_CACHE_SIZE];
int arp_cache_count = 0;

// 全局变量用于信号处理
static volatile int keep_running = 1;
static int forward_count = 0;
static int reverse_count = 0;

// 统计信息
struct {
    int total_packets;
    int forward_packets;  // 1->2方向
    int reverse_packets;  // 2->1方向
    int dropped_packets;
    int ttl_expired;
    int no_route;
} stats = {0};

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

// 添加ARP缓存条目
void add_arp_entry(uint32_t ip_addr, unsigned char *mac) {
    // 检查是否已存在
    for (int i = 0; i < arp_cache_count; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip_addr == ip_addr) {
            memcpy(arp_cache[i].mac, mac, ETH_ALEN);
            return;
        }
    }

    // 添加新条目
    if (arp_cache_count < ARP_CACHE_SIZE) {
        arp_cache[arp_cache_count].ip_addr = ip_addr;
        memcpy(arp_cache[arp_cache_count].mac, mac, ETH_ALEN);
        arp_cache[arp_cache_count].valid = 1;
        arp_cache_count++;
    }
}

// 查找ARP缓存
unsigned char *lookup_arp(uint32_t ip_addr) {
    for (int i = 0; i < arp_cache_count; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip_addr == ip_addr) {
            return arp_cache[i].mac;
        }
    }
    return NULL;
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

// 初始化路由表（在 main() 中调用，使用运行时初始化）
void init_route_table() {
    // 路由条目 0: 192.168.2.0/24 -> ens37 (正向转发: 1->2)
    route_table[0].dest = inet_addr("192.168.2.0");
    route_table[0].gateway = 0;  // 直接交付
    route_table[0].netmask = inet_addr("255.255.255.0");
    strncpy(route_table[0].interface, "ens37", IFNAMSIZ - 1);
    route_table[0].interface[IFNAMSIZ - 1] = '\0';
    strncpy(route_table[0].description, "Forward: 1->2", sizeof(route_table[0].description) - 1);
    route_table[0].description[sizeof(route_table[0].description) - 1] = '\0';

    // 路由条目 1: 192.168.1.0/24 -> ens38 (反向转发: 2->1)
    route_table[1].dest = inet_addr("192.168.1.0");
    route_table[1].gateway = 0;  // 直接交付
    route_table[1].netmask = inet_addr("255.255.255.0");
    strncpy(route_table[1].interface, "ens38", IFNAMSIZ - 1);
    route_table[1].interface[IFNAMSIZ - 1] = '\0';
    strncpy(route_table[1].description, "Reverse: 2->1", sizeof(route_table[1].description) - 1);
    route_table[1].description[sizeof(route_table[1].description) - 1] = '\0';
}

// 判断转发方向
const char* get_direction(uint32_t src_ip, uint32_t dest_ip) {
    if ((src_ip & inet_addr("255.255.255.0")) == inet_addr("192.168.1.0") &&
        (dest_ip & inet_addr("255.255.255.0")) == inet_addr("192.168.2.0")) {
        return "FORWARD (1→2)";
    } else if ((src_ip & inet_addr("255.255.255.0")) == inet_addr("192.168.2.0") &&
               (dest_ip & inet_addr("255.255.255.0")) == inet_addr("192.168.1.0")) {
        return "REVERSE (2→1)";
    }
    return "UNKNOWN";
}

// 初始化ARP缓存（手动配置常用主机）
void init_arp_cache() {
    // 可以在这里添加静态ARP条目
    // 例如：add_arp_entry(inet_addr("192.168.2.2"), (unsigned char[]){0x00, 0x0c, 0x29, 0x48, 0xd3, 0xf7});
    
    printf("ARP缓存已初始化\n");
    printf("提示：首次转发时会使用广播MAC地址\n\n");
}

// 显示路由表
void display_route_table() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                        路由表                                 ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║ 目的网络          子网掩码          接口    描述             ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < route_table_size; i++) {
        struct in_addr dest, mask;
        dest.s_addr = route_table[i].dest;
        mask.s_addr = route_table[i].netmask;
        printf("║ %-17s %-17s %-7s %-16s║\n",
               inet_ntoa(dest), inet_ntoa(mask), 
               route_table[i].interface, route_table[i].description);
    }
    
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
}

// 显示统计信息
void display_stats() {
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                      转发统计信息                             ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║ 总接收包数:       %-10d                                ║\n", stats.total_packets);
    printf("║ 正向转发(1→2):    %-10d                                ║\n", stats.forward_packets);
    printf("║ 反向转发(2→1):    %-10d                                ║\n", stats.reverse_packets);
    printf("║ TTL过期丢弃:      %-10d                                ║\n", stats.ttl_expired);
    printf("║ 无路由丢弃:       %-10d                                ║\n", stats.no_route);
    printf("║ 其他丢弃:         %-10d                                ║\n", stats.dropped_packets);
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
}

int main() {
    int sockfd;
    struct sockaddr saddr;
    unsigned char *buffer = (unsigned char *)malloc(BUFFER_SIZE);
    char time_str[20];

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          双网口路由转发程序 - 增强版（支持双向转发）         ║\n");
    printf("║                   (实验4.3 Enhanced)                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("接口配置:\n");
    printf("  eth0: 192.168.1.1/24 (连接源主机网段)\n");
    printf("  eth1: 192.168.2.1/24 (连接目的主机网段)\n\n");

    // 初始化路由表（运行时配置）
    init_route_table();

    // 显示路由表
    display_route_table();

    printf("⚠️  需要ROOT权限运行！\n");
    printf("===========================================\n\n");

    // 注册信号处理
    signal(SIGINT, signal_handler);

    // 初始化ARP缓存
    init_arp_cache();

    // 创建原始套接字
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd < 0) {
        perror("socket");
        printf("\n错误：需要ROOT权限！请使用 sudo 运行\n");
        free(buffer);
        return 1;
    }

    printf("开始监听数据包... (按 Ctrl+C 退出)\n");
    printf("支持双向转发: 192.168.1.x ↔ 192.168.2.x\n");
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

                // 双向过滤：处理两个方向的数据包
        uint32_t mask = inet_addr("255.255.255.0");
        uint32_t net_1 = inet_addr("192.168.1.0");
        uint32_t net_2 = inet_addr("192.168.2.0");
        
        int is_forward = ((ip_header->saddr & mask) == net_1 &&
                         (ip_header->daddr & mask) == net_2);
        
        int is_reverse = ((ip_header->saddr & mask) == net_2 &&
                         (ip_header->daddr & mask) == net_1);

        if (!is_forward && !is_reverse) {
            continue;
        }

        stats.total_packets++;

        // 查找路由表
        struct route_entry *route = lookup_route(ip_header->daddr);
        if (route == NULL) {
            fprintf(stderr, "No route to host %s\n", dest_ip);
            stats.no_route++;
            continue;
        }

        get_time_string(time_str, sizeof(time_str));

        printf("┌─────────────────────────────────────────────────────────────┐\n");
        printf("│ [%s] 数据包 #%-4d - %s\n", 
               time_str, stats.total_packets, get_direction(ip_header->saddr, ip_header->daddr));
        printf("├─────────────────────────────────────────────────────────────┤\n");
        printf("│ 源地址:       %-45s │\n", src_ip);
        printf("│ 目的地址:     %-45s │\n", dest_ip);
        printf("│ 原TTL:        %-45d │\n", ip_header->ttl);
        printf("│ 协议:         %-45s │\n", "UDP");

        // 检查TTL
        if (ip_header->ttl <= 1) {
            printf("│ 状态:       ✗ TTL过期，丢弃数据包                           │\n");
            printf("└─────────────────────────────────────────────────────────────┘\n\n");
            stats.ttl_expired++;
            continue;
        }

        // 修改TTL
        ip_header->ttl -= 1;
        ip_header->check = 0;
        ip_header->check = checksum((unsigned short *)ip_header, ip_header->ihl * 4);

        printf("│ 新TTL:        %-45d │\n", ip_header->ttl);
        printf("│ 出接口:       %-45s │\n", route->interface);
        printf("│ 路由:         %-45s │\n", route->description);

        // 获取出接口信息
        struct ifreq ifr, ifr_mac;
        struct sockaddr_ll dest;

        // 获取网卡接口索引
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, route->interface, IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            perror("ioctl SIOCGIFINDEX");
            stats.dropped_packets++;
            continue;
        }

        // 获取网卡接口MAC地址
        memset(&ifr_mac, 0, sizeof(ifr_mac));
        strncpy(ifr_mac.ifr_name, route->interface, IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFHWADDR, &ifr_mac) < 0) {
            perror("ioctl SIOCGIFHWADDR");
            stats.dropped_packets++;
            continue;
        }

        // 查找目标MAC地址
        unsigned char *target_mac = lookup_arp(ip_header->daddr);
        unsigned char broadcast_mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        
        // 如果没有找到，使用广播地址（实际应用中应该发送ARP请求）
        if (target_mac == NULL) {
            target_mac = broadcast_mac;
            printf("│ 目标MAC:    广播 (ff:ff:ff:ff:ff:ff)                        │\n");
        } else {
            printf("│ 目标MAC:      %02x:%02x:%02x:%02x:%02x:%02x (从ARP缓存)                 │\n",
                   target_mac[0], target_mac[1], target_mac[2],
                   target_mac[3], target_mac[4], target_mac[5]);
        }

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
            perror("sendto failed");
            printf("│ 状态:       ✗ 转发失败                                      │\n");
            stats.dropped_packets++;
        } else {
            printf("│ 状态:       ✓ 转发成功                                      │\n");
            
            // 更新统计
            if (is_forward) {
                stats.forward_packets++;
            } else if (is_reverse) {
                stats.reverse_packets++;
            }
            
            // 学习源MAC地址到ARP缓存
            add_arp_entry(ip_header->saddr, eth_header->h_source);
        }

        printf("└─────────────────────────────────────────────────────────────┘\n\n");
    }

    // 显示统计信息
    display_stats();

    close(sockfd);
    free(buffer);
    return 0;
}
