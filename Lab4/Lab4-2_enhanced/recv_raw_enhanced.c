/*
 * 实验4.2增强版 - 接收端程序
 * 功能：接收通过转发表转发来的UDP数据报
 * 编译：gcc -o recv_raw_enhanced recv_raw_enhanced.c
 * 运行：./recv_raw_enhanced
 *
 * 改进点：
 * 1. 增强的消息显示界面
 * 2. 显示来源主机识别
 * 3. 统计信息显示
 * 4. 支持多接收端场景
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
#define RECV_PORT 12345

// 已知发送端配置（用于显示友好名称）
typedef struct {
    char ip[16];
    char name[32];
} KnownHost;

KnownHost known_senders[] = {
    {"192.168.10.100", "Camellya"}
};

const int num_known_senders = sizeof(known_senders) / sizeof(KnownHost);

// 统计信息
typedef struct {
    int total_received;
    int from_camellya;
    int from_others;
} RecvStats;

RecvStats stats = {0, 0, 0};

// 全局变量用于信号处理
static volatile int keep_running = 1;

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

// 根据IP查找主机名
const char* get_sender_name(const char *ip) {
    for (int i = 0; i < num_known_senders; i++) {
        if (strcmp(known_senders[i].ip, ip) == 0) {
            return known_senders[i].name;
        }
    }
    return "Unknown";
}

// 更新统计信息
void update_stats(const char *ip) {
    stats.total_received++;

    if (strcmp(ip, "192.168.10.100") == 0) {
        stats.from_camellya++;
    } else {
        stats.from_others++;
    }
}

// 显示统计信息
void print_statistics() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                         接收统计信息                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ 总接收消息数: %-10d                                       ║\n", stats.total_received);
    printf("║ 来自Camellya: %-10d                                       ║\n", stats.from_camellya);
    printf("║ 来自其他:     %-10d                                       ║\n", stats.from_others);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char time_str[20];
    char hostname[256];

    // 获取本机主机名
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "Unknown Host");
    }

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

    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║            实验4.2增强版 - UDP消息接收程序                     ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ 主机名: %-54s ║\n", hostname);
    printf("║ 监听端口: %-10d                                           ║\n", RECV_PORT);
    printf("║ 等待接收消息... (按 Ctrl+C 退出)                               ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    // 持续接收消息
    while (keep_running) {
        memset(buffer, 0, sizeof(buffer));

        // 设置接收超时，以便能响应信号
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int recv_len = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr *)&client_addr, &addr_len);

        if (recv_len < 0) {
            continue;
        }

        buffer[recv_len] = '\0';

        // 获取当前时间
        get_time_string(time_str, sizeof(time_str));

        // 获取发送者IP和名称
        char *sender_ip = inet_ntoa(client_addr.sin_addr);
        const char *sender_name = get_sender_name(sender_ip);

        // 更新统计
        update_stats(sender_ip);

        // 显示接收到的消息
        printf("╔════════════════════════════════════════════════════════════════╗\n");
        printf("║ 消息 #%-3d                                           [%s] ║\n",
               stats.total_received, time_str);
        printf("╠════════════════════════════════════════════════════════════════╣\n");
        printf("║ 来源主机: %-15s (%-15s)                    ║\n",
               sender_name, sender_ip);
        printf("║ 来源端口: %-10d                                           ║\n",
               ntohs(client_addr.sin_port));
        printf("║ 消息长度: %-10d 字节                                      ║\n",
               recv_len);
        printf("╠════════════════════════════════════════════════════════════════╣\n");
        printf("║ 消息内容:                                                      ║\n");

        // 处理多行消息显示
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            printf("║ %-62s ║\n", line);
            line = strtok(NULL, "\n");
        }

        printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    }

    // 显示统计信息
    print_statistics();

    printf("\n程序正常退出\n");
    close(sockfd);
    return 0;
}
