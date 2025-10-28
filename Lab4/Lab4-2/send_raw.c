/*
 * 实验4.2 - 发送端程序（主机Camellya）
 * 功能：使用AF_PACKET原始套接字构造完整的以太网帧发送UDP数据报
 * 编译：gcc -o send_raw send_raw.c
 * 运行：sudo ./send_raw
 *
 * 注意：
 * 1. 需要ROOT权限运行
 * 2. 需要修改目标MAC地址为转发主机的实际MAC地址
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

#define BUFFER_SIZE 1518
#define UDP_SRC_PORT 12345
#define UDP_DST_PORT 12345

// 网络配置
#define SRC_IP "192.168.10.100"    // Camellya的IP
#define DST_IP "192.168.10.102"    // Jinhsi的IP (最终目的地)
#define INTERFACE "ens33"           // 网卡接口名，需要根据实际情况修改

// ⚠️ 重要：需要修改为转发主机Shorekeeper的实际MAC地址
// 使用命令查看：在Shorekeeper上执行 ip a
// 00:50:56:22:13:0a
#define DEST_MAC0 0x00
#define DEST_MAC1 0x50
#define DEST_MAC2 0x56
#define DEST_MAC3 0x22
#define DEST_MAC4 0x13
#define DEST_MAC5 0x0a

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
    char message[1024];
    int msg_count = 0;

    printf("===========================================\n");
    printf("  原始套接字UDP发送程序 - Camellya\n");
    printf("===========================================\n");
    printf("源IP: %s\n", SRC_IP);
    printf("目的IP: %s (Jinhsi)\n", DST_IP);
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

    printf("本机MAC地址: %02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned char)if_mac.ifr_hwaddr.sa_data[0],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[1],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[2],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[3],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[4],
           (unsigned char)if_mac.ifr_hwaddr.sa_data[5]);

    printf("目标MAC地址: %02x:%02x:%02x:%02x:%02x:%02x (Shorekeeper)\n",
           DEST_MAC0, DEST_MAC1, DEST_MAC2, DEST_MAC3, DEST_MAC4, DEST_MAC5);
    printf("\n输入消息进行发送，输入 'quit' 或 'exit' 退出\n");
    printf("===========================================\n\n");

    // 循环发送消息
    while (1) {
        printf("请输入消息 [%d]: ", msg_count + 1);
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

        // 检查退出命令
        if (strcmp(message, "quit") == 0 || strcmp(message, "exit") == 0) {
            printf("\n程序退出\n");
            break;
        }

        // 检查空消息
        if (len == 0) {
            printf("消息不能为空\n");
            continue;
        }

        // 清空缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 构造以太网帧头
        struct ether_header *eh = (struct ether_header *)buffer;
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
        eh->ether_type = htons(ETH_P_IP);

        // 构造IP头
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
        iph->saddr = inet_addr(SRC_IP);
        iph->daddr = inet_addr(DST_IP);  // 注意：这里是最终目的地Jinhsi的IP
        iph->check = checksum((unsigned short *)iph, sizeof(struct iphdr));

        // 构造UDP头
        struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct ether_header) +
                                                 sizeof(struct iphdr));
        udph->source = htons(UDP_SRC_PORT);
        udph->dest = htons(UDP_DST_PORT);
        udph->len = htons(sizeof(struct udphdr) + strlen(message));
        udph->check = 0;  // UDP校验和可选

        // 填充数据
        char *data = (char *)(buffer + sizeof(struct ether_header) +
                             sizeof(struct iphdr) + sizeof(struct udphdr));
        strcpy(data, message);

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

        // 发送数据包
        int total_len = sizeof(struct ether_header) + sizeof(struct iphdr) +
                       sizeof(struct udphdr) + strlen(message);

        if (sendto(sockfd, buffer, total_len, 0,
                   (struct sockaddr *)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
            perror("sendto");
            continue;
        }

        msg_count++;
        printf("✓ 数据包已发送 [%d]: \"%s\" (%ld 字节)\n", msg_count, message, strlen(message));
        printf("  总帧长度: %d 字节 (ETH:%lu + IP:%lu + UDP:%lu + DATA:%ld)\n\n",
               total_len, sizeof(struct ether_header), sizeof(struct iphdr),
               sizeof(struct udphdr), strlen(message));

        usleep(100000);  // 0.1秒延迟
    }

    printf("\n===========================================\n");
    printf("总共发送了 %d 个数据包\n", msg_count);
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
