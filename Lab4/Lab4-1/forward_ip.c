/*
 * 实验4.1 - 改进版转发端程序（主机Shorekeeper）
 * 功能：持续接收来自发送主机的UDP数据报，并转发给接收主机
 * 编译：gcc -o forward_ip_ad forward_ip_ad.c
 * 运行：./forward_ip_ad
 * 注意：需要先开放防火墙端口：sudo ufw allow 12345/udp
 * 改进点：
 *   - 支持持续接收和转发多条消息
 *   - 显示消息统计信息
 *   - 按 Ctrl+C 退出程序
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define RECV_PORT 12345      // 接收端口（从Camellya接收）
#define FORWARD_PORT 54321   // 转发端口（发送给Jinhsi）
#define DEST_IP "192.168.10.102"  // 接收主机Jinhsi的IP

// 全局变量用于信号处理
static volatile int keep_running = 1;
static int msg_count = 0;

// 信号处理函数
void signal_handler(int signum) {
    printf("\n\n接收到退出信号 (Ctrl+C)\n");
    keep_running = 0;
}

int main() {
    int sockfd;
    struct sockaddr_in src_addr, dest_addr, my_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len;

    // 注册信号处理
    signal(SIGINT, signal_handler);

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 配置本地地址（绑定到接收端口）
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(RECV_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字到本地地址
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    // 配置目标地址（接收主机的地址）
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(FORWARD_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

    printf("===========================================\n");
    printf("  UDP 消息转发程序 - 主机 Shorekeeper\n");
    printf("===========================================\n");
    printf("监听端口: %d (接收来自 Camellya)\n", RECV_PORT);
    printf("转发目标: %s:%d (Jinhsi)\n", DEST_IP, FORWARD_PORT);
    printf("等待接收消息... (按 Ctrl+C 退出)\n");
    printf("===========================================\n\n");

    // 持续接收和转发消息
    while (keep_running) {
        memset(buffer, 0, sizeof(buffer));
        addr_len = sizeof(src_addr);

        // 接收数据报（设置超时以便能响应信号）
        struct timeval tv;
        tv.tv_sec = 1;  // 1秒超时
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&src_addr, &addr_len);

        if (recv_len < 0) {
            // 如果是超时，继续循环
            continue;
        }

        buffer[recv_len] = '\0';
        msg_count++;

        printf("[消息 %d] 收到来自 %s:%d\n",
               msg_count, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
        printf("  内容: \"%s\"\n", buffer);
        printf("  长度: %d 字节\n", recv_len);

        // 转发数据报到接收主机
        if (sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("  ✗ 转发失败");
            continue;
        }

        printf("  ✓ 已转发到 %s:%d\n\n", DEST_IP, FORWARD_PORT);
    }

    printf("\n===========================================\n");
    printf("总共转发了 %d 条消息\n", msg_count);
    printf("程序正常退出\n");
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
