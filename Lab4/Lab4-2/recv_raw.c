/*
 * 实验4.2 - 接收端程序（主机Jinhsi）
 * 功能：接收通过原始套接字转发来的UDP数据报
 * 编译：gcc -o recv_raw recv_raw.c
 * 运行：./recv_raw
 * 注意：需要先开放防火墙端口：sudo ufw allow 12345/udp
 *
 * 说明：虽然发送端和转发端使用原始套接字，但接收端可以使用标准UDP套接字
 *      因为数据包到达时已经是完整的UDP数据报
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define RECV_PORT 12345  // 接收端口

// 全局变量用于信号处理
static volatile int keep_running = 1;
static int msg_count = 0;

// 信号处理函数
void signal_handler(int signum) {
    printf("\n\n接收到退出信号 (Ctrl+C)\n");
    keep_running = 0;
}

// 获取当前时间字符串
void get_time_string(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, size, "%H:%M:%S", t);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char time_str[20];

    // 注册信号处理
    signal(SIGINT, signal_handler);

    // 创建 UDP 套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // 配置本地地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(RECV_PORT);

    // 绑定套接字到本地地址
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    printf("===========================================\n");
    printf("  UDP 消息接收程序 - 主机 Jinhsi\n");
    printf("  (实验4.2 - 原始套接字转发实验)\n");
    printf("===========================================\n");
    printf("监听端口: %d\n", RECV_PORT);
    printf("等待接收消息... (按 Ctrl+C 退出)\n");
    printf("===========================================\n\n");

    // 持续接收消息
    while (keep_running) {
        memset(buffer, 0, sizeof(buffer));

        // 设置接收超时，以便能响应信号
        struct timeval tv;
        tv.tv_sec = 1;  // 1秒超时
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&client_addr, &addr_len);

        if (recv_len < 0) {
            // 如果是超时，继续循环
            continue;
        }

        buffer[recv_len] = '\0';
        msg_count++;

        // 获取当前时间
        get_time_string(time_str, sizeof(time_str));

        printf("╔═══════════════════════════════════════╗\n");
        printf("║ 消息 #%-3d                     [%s] ║\n", msg_count, time_str);
        printf("╠═══════════════════════════════════════╣\n");
        printf("║ 来源: %-31s ║\n", inet_ntoa(client_addr.sin_addr));
        printf("║ 端口: %-31d ║\n", ntohs(client_addr.sin_port));
        printf("║ 长度: %-31d ║\n", recv_len);
        printf("╠═══════════════════════════════════════╣\n");
        printf("║ 消息内容:                            ║\n");
        printf("║ %-37s ║\n", buffer);
        printf("╚═══════════════════════════════════════╝\n\n");
    }

    printf("\n===========================================\n");
    printf("总共接收了 %d 条消息\n", msg_count);
    printf("程序正常退出\n");
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
