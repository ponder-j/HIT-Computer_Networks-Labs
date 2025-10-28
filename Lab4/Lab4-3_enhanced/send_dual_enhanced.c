/*
 * 实验4.3增强版 - 发送端程序（源主机）
 * 功能：发送UDP数据包到不同子网的目的主机，支持接收回复
 * 编译：gcc -o send_dual_enhanced send_dual_enhanced.c -lpthread
 * 运行：./send_dual_enhanced
 *
 * 网络配置：
 * 源主机（本机）：192.168.1.2/24
 * 路由主机（网关）：192.168.1.1/24 (ens37) 和 192.168.2.1/24 (ens38)
 * 目的主机：192.168.2.2/24
 *
 * 增强特性：
 * 1. 支持双向通信（发送和接收）
 * 2. 多线程处理（一个线程发送，一个线程接收）
 * 3. 消息序列号跟踪
 * 4. 响应时间测量
 * 5. 增强的日志输出
 *
 * 注意：需要配置默认网关：sudo ip route add default via 192.168.1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define DEST_IP "192.168.2.2"   // 目的主机IP（在不同子网）
#define DEST_PORT 12345
#define LOCAL_PORT 54321

// 全局变量
static volatile int keep_running = 1;
static int sockfd;
static int msg_sent = 0;
static int msg_received = 0;

// 消息结构
struct message {
    int seq_num;
    struct timeval timestamp;
    char content[BUFFER_SIZE - sizeof(int) - sizeof(struct timeval)];
};

// 信号处理函数
void signal_handler(int signum) {
    printf("\n\n接收到退出信号...\n");
    keep_running = 0;
}

// 获取当前时间字符串
void get_time_string(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, size, "%H:%M:%S", t);
}

// 计算时间差（毫秒）
long time_diff_ms(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000 + 
           (end->tv_usec - start->tv_usec) / 1000;
}

// 接收线程函数
void *receive_thread(void *arg) {
    struct sockaddr_in from_addr;
    socklen_t addr_len = sizeof(from_addr);
    char buffer[BUFFER_SIZE];
    char time_str[20];

    printf("[接收线程] 已启动，监听端口 %d\n", LOCAL_PORT);
    printf("[接收线程] 等待接收来自目的主机的响应...\n\n");

    while (keep_running) {
        memset(buffer, 0, sizeof(buffer));

        // 设置接收超时
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&from_addr, &addr_len);

        if (recv_len < 0) {
            continue;  // 超时，继续等待
        }

        struct message *msg = (struct message *)buffer;
        struct timeval now;
        gettimeofday(&now, NULL);
        long rtt = time_diff_ms(&msg->timestamp, &now);

        msg_received++;
        get_time_string(time_str, sizeof(time_str));

        printf("\n╔═══════════════════════════════════════════════════════════╗\n");
        printf("║ [%s] 收到响应 #%-4d                                 ║\n", time_str, msg_received);
        printf("╠═══════════════════════════════════════════════════════════╣\n");
        printf("║ 来源:        %-44s ║\n", inet_ntoa(from_addr.sin_addr));
        printf("║ 端口:        %-44d ║\n", ntohs(from_addr.sin_port));
        printf("║ 序列号:      %-44d ║\n", msg->seq_num);
        printf("║ RTT:         %-41ld ms ║\n", rtt);
        printf("║ 内容:        %-44s ║\n", msg->content);
        printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    }

    printf("[接收线程] 已退出\n");
    return NULL;
}

int main() {
    struct sockaddr_in local_addr, dest_addr;
    char message[BUFFER_SIZE];
    pthread_t recv_thread;
    char time_str[20];

    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║        UDP 双向通信程序 - 源主机 (增强版)                 ║\n");
    printf("║              (实验4.3 Enhanced)                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    printf("网络配置:\n");
    printf("  本机网段:   192.168.1.0/24\n");
    printf("  目标主机:   %s:%d (192.168.2.0/24)\n", DEST_IP, DEST_PORT);
    printf("  本地端口:   %d (用于接收响应)\n", LOCAL_PORT);
    printf("  通过网关:   192.168.1.1\n");
    printf("===========================================\n\n");

    // 注册信号处理
    signal(SIGINT, signal_handler);

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 绑定本地地址（用于接收响应）
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(LOCAL_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    // 设置目标地址
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

    // 创建接收线程
    if (pthread_create(&recv_thread, NULL, receive_thread, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        return 1;
    }

    printf("输入消息进行发送，输入 'quit' 或 'exit' 退出\n");
    printf("支持双向通信：发送消息并接收响应\n");
    printf("===========================================\n\n");

    // 循环读取用户输入并发送
    while (keep_running) {
        sleep(0.5);
        printf("请输入消息 [%d]: ", msg_sent + 1);
        fflush(stdout);

        // 读取用户输入
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
            if (keep_running) {
                printf("\n读取输入失败\n");
            }
            break;
        }

        // 移除换行符
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') {
            message[len - 1] = '\0';
            len--;
        }

        // 检查是否退出
        if (strcmp(message, "quit") == 0 || strcmp(message, "exit") == 0) {
            printf("\n准备退出...\n");
            break;
        }

        // 检查空消息
        if (len == 0) {
            printf("消息不能为空，请重新输入\n\n");
            continue;
        }

        // 构造消息
        struct message msg;
        msg.seq_num = msg_sent + 1;
        gettimeofday(&msg.timestamp, NULL);
        strncpy(msg.content, message, sizeof(msg.content) - 1);
        msg.content[sizeof(msg.content) - 1] = '\0';

        // 发送数据报
        if (sendto(sockfd, &msg, sizeof(msg), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("sendto");
            continue;
        }

        msg_sent++;
        get_time_string(time_str, sizeof(time_str));

        printf("┌───────────────────────────────────────────────────────────┐\n");
        printf("│ [%s] ✓ 消息已发送 #%-4d                             │\n", time_str, msg_sent);
        printf("├───────────────────────────────────────────────────────────┤\n");
        printf("│ 目标:        %s:%-30d   │\n", DEST_IP, DEST_PORT);
        printf("│ 序列号:      %-44d │\n", msg.seq_num);
        printf("│ 内容:        %-44s │\n", msg.content);
        printf("│ 长度:        %-41ld字节│\n", sizeof(msg));
        printf("└───────────────────────────────────────────────────────────┘\n\n");

        // 短暂延迟
        usleep(100000); // 0.1秒
    }

    // 等待接收线程结束
    keep_running = 0;
    printf("\n等待接收线程结束...\n");
    pthread_join(recv_thread, NULL);

    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                    通信统计信息                           ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ 发送消息数:   %-42d  ║\n", msg_sent);
    printf("║ 接收消息数:   %-42d  ║\n", msg_received);
    // printf("║ 丢包率:       %-39.1f%% ║\n", 
    //        msg_sent > 0 ? (1.0 - (double)msg_received / msg_sent) * 100 : 0.0);
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    close(sockfd);
    return 0;
}
