/*
 * 实验4.3 - 发送端程序（源主机）
 * 功能：发送UDP数据包到不同子网的目的主机
 * 编译：gcc -o send_dual send_dual.c
 * 运行：./send_dual
 *
 * 网络配置：
 * 源主机（本机）：192.168.1.2/24
 * 路由主机（网关）：192.168.1.1/24 (ens37) 和 192.168.2.1/24 (ens38)
 * 目的主机：192.168.2.2/24
 *
 * 注意：需要配置默认网关：sudo ip route add default via 192.168.1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define DEST_IP "192.168.2.2"   // 目的主机IP（在不同子网）
#define DEST_PORT 12345

int main() {
    int sockfd;
    struct sockaddr_in dest_addr;
    char message[BUFFER_SIZE];
    int msg_count = 0;

    printf("===========================================\n");
    printf("  UDP 消息发送程序 - 源主机\n");
    printf("  (实验4.3 - 双网口路由转发)\n");
    printf("===========================================\n");
    printf("本机应在: 192.168.1.0/24 网段\n");
    printf("目标主机: %s:%d (192.168.2.0/24网段)\n", DEST_IP, DEST_PORT);
    printf("通过网关: 192.168.1.1 (路由主机)\n");
    printf("输入消息进行发送，输入 'quit' 或 'exit' 退出\n");
    printf("===========================================\n\n");

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 1;
    }

    // 设置目标地址（目的主机的地址）
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

    // 循环读取用户输入并发送
    while (1) {
        printf("请输入消息 [%d]: ", msg_count + 1);
        fflush(stdout);

        // 读取用户输入
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
            printf("\n读取输入失败，程序退出\n");
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
            printf("\n程序退出\n");
            break;
        }

        // 检查空消息
        if (len == 0) {
            printf("消息不能为空，请重新输入\n");
            continue;
        }

        // 发送数据报
        if (sendto(sockfd, message, strlen(message), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("sendto");
            continue;
        }

        msg_count++;
        printf("✓ 消息已发送 [%d]: \"%s\" (%ld 字节)\n",
               msg_count, message, strlen(message));
        printf("  -> 目标: %s:%d (跨子网通信)\n\n", DEST_IP, DEST_PORT);

        // 短暂延迟，避免发送过快
        usleep(100000); // 0.1秒
    }

    printf("\n===========================================\n");
    printf("总共发送了 %d 条消息\n", msg_count);
    printf("===========================================\n");

    close(sockfd);
    return 0;
}
